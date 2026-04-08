#include "tank.h"

Tank::Tank(b2World& world, int _playerIndex, int _fw, int _bw, int _tl, int _tr, int _sh, int _shield) {
    playerIndex = _playerIndex;
    forwardKey = (_fw != 0) ? _fw : KEY_W;
    backwardKey = (_bw != 0) ? _bw : KEY_S;
    turnLeftKey = (_tl != 0) ? _tl : KEY_A;
    turnRightKey = (_tr != 0) ? _tr : KEY_D;
    shootKey = (_sh != 0) ? _sh : KEY_Q;
    shieldKey = (_shield != 0) ? _shield : KEY_E;
    shootCooldownTimer = 0.0f;
    isDestroyed = false;
    currentWeapon = ItemType::NORMAL;
    ammo = 0;
    hasShield = false;
    shieldTimer = 0.0f;
    shieldCooldownTimer = 0.0f;

    b2BodyDef tankDef;
    tankDef.type = b2_dynamicBody;
    tankDef.position.Set((SCREEN_WIDTH / 2.0f) / SCALE, (SCREEN_HEIGHT / 2.0f) / SCALE);
    tankDef.fixedRotation = false;
    body = world.CreateBody(&tankDef);

    b2PolygonShape hullShape;
    hullShape.SetAsBox(14.0f / SCALE, 14.0f / SCALE, b2Vec2(0.0f, -7.0f / SCALE), 0.0f);
    b2FixtureDef hullFix; hullFix.shape = &hullShape; hullFix.density = 1.0f; hullFix.friction = 0.0f; hullFix.restitution = 0.0f;
    body->CreateFixture(&hullFix);

    b2PolygonShape barrelShape;
    barrelShape.SetAsBox(3.0f / SCALE, 7.0f / SCALE, b2Vec2(0.0f, 14.0f / SCALE), 0.0f);
    b2FixtureDef barrelFix; barrelFix.shape = &barrelShape; barrelFix.density = 0.2f; barrelFix.friction = 0.0f; barrelFix.restitution = 0.0f;
    body->CreateFixture(&barrelFix);
}

void Tank::Update(b2World& world, std::vector<Bullet*>& bullets, std::vector<Item*>& items, float dt) {
    // 1. Cập nhật các bộ đếm thời gian
    if (shieldCooldownTimer > 0.0f) shieldCooldownTimer -= dt;
    if (shieldTimer > 0.0f) {
        shieldTimer -= dt;
        if (shieldTimer <= 0.0f) hasShield = false;
    }
    if (shootCooldownTimer > 0.0f) shootCooldownTimer -= dt;

    if (IsKeyPressed(shieldKey) && shieldCooldownTimer <= 0.0f) {
        hasShield = true;
        shieldTimer = 5.0f;
        shieldCooldownTimer = 15.0f; 
    }

    // 2. Chạy logic xử lý phân mảnh
    HandleMovement();
    FireWeapon(world, bullets);
    CheckCollisions(bullets, items);
}

void Tank::HandleMovement() {
    float moveSpeed = 3.0f, turnSpeed = 3.0f;
    float angularVel = 0.0f;
    
    if (IsKeyDown(turnLeftKey)) angularVel += turnSpeed;
    if (IsKeyDown(turnRightKey)) angularVel -= turnSpeed;
    body->SetAngularVelocity(angularVel);

    b2Vec2 vel(0.0f, 0.0f);
    float currentAngle = body->GetAngle();
    b2Vec2 forwardDir(-sinf(currentAngle), cosf(currentAngle));

    if (IsKeyDown(forwardKey)) { vel.x += forwardDir.x * moveSpeed; vel.y += forwardDir.y * moveSpeed; }
    if (IsKeyDown(backwardKey)) { vel.x -= forwardDir.x * moveSpeed; vel.y -= forwardDir.y * moveSpeed; }
    body->SetLinearVelocity(vel);
}

// Cấu trúc Raycast để kiểm tra đường đạn trước khi bắn xem có bị kẹt vào tường không
class WallRayCastCallback : public b2RayCastCallback {
public:
    bool hitWall = false;
    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        // Chỉ coi là tường nếu đó là vật thể Static
        if (fixture->GetBody()->GetType() == b2_staticBody) { 
            hitWall = true; 
            return fraction; 
        }
        return -1.0f; // Bỏ qua các vật thể khác
    }
};

void Tank::FireWeapon(b2World& world, std::vector<Bullet*>& bullets) {
    int activeMyBullets = 0;
    for (Bullet* b : bullets) {
        if (b->ownerPlayerIndex == playerIndex && !b->isMissile && !b->isFrag) activeMyBullets++;
    }

    if (IsKeyPressed(shootKey) && shootCooldownTimer <= 0.0f) {
        float currentAngle = body->GetAngle();
        b2Vec2 forwardDir(-sinf(currentAngle), cosf(currentAngle));
        b2Vec2 startPos = body->GetPosition();
        b2Vec2 spawnPos = startPos + (30.0f / SCALE) * forwardDir;
        
        WallRayCastCallback callback;
        world.RayCast(&callback, startPos, spawnPos);
        
        if (!callback.hitWall) {
            bool hasFragActive = false;
            
            // Xử lý kích nổ mìn Frag nếu đã thả
            for (Bullet* b : bullets) {
                if (b->isFrag && b->ownerPlayerIndex == playerIndex && !b->explodeFrag) {
                    b->explodeFrag = true;
                    hasFragActive = true;
                    // Lần kích nổ mới được tính là dùng đạn (để cho phép giấu mìn chờ kích nổ)
                    if (currentWeapon == ItemType::FRAG && ammo > 0) {
                        ammo--;
                        if (ammo <= 0) currentWeapon = ItemType::NORMAL;
                    }
                    shootCooldownTimer = 0.5f;
                    break;
                }
            }

            // Nếu không kích nổ mìn, thực hiện bắn đạn mới
            if (!hasFragActive) {
                switch (currentWeapon) {
                    case ItemType::GATLING: {
                        // Bắn dạng chùm (Shotgun) với 5 viên văng hình quạt
                        for (int i = 0; i < 5; i++) {
                            float angleOffset = (i - 2) * 0.15f; 
                            float cosA = cosf(angleOffset);
                            float sinA = sinf(angleOffset);
                            b2Vec2 dir(forwardDir.x * cosA - forwardDir.y * sinA, forwardDir.x * sinA + forwardDir.y * cosA);
                            Bullet* b = new Bullet(world, spawnPos, 10.0f * dir, false, false, false, playerIndex);
                            b->time = 1.0f; 
                            bullets.push_back(b);
                        }
                        shootCooldownTimer = 0.5f;
                        break;
                    }
                    case ItemType::FRAG: {
                        // Thả 1 viên mìn to có thể kích nổ
                        bullets.push_back(new Bullet(world, spawnPos, 5.0f * forwardDir, false, true, false, playerIndex));
                        shootCooldownTimer = 0.5f;
                        // Lưu ý: Frag giảm ammo ở bước kích nổ (phía trên), không giảm ở đây
                        break;
                    }
                    case ItemType::MISSILE: {
                        // Bắn tên lửa điều hướng bám theo đối thủ
                        bullets.push_back(new Bullet(world, spawnPos, 4.5f * forwardDir, false, false, true, playerIndex));
                        shootCooldownTimer = 0.5f;
                        break;
                    }
                    case ItemType::DEATH_RAY: {
                        // Bắn tia lase có tốc độ bay cực lớn và sức công phá mạnh
                        bullets.push_back(new Bullet(world, spawnPos, 8.0f * forwardDir, true, false, false, playerIndex));
                        shootCooldownTimer = 0.5f;
                        break;
                    }
                    default: {
                        // Đạn súng mặc định (tối đa giới hạn 5 viên cùng lúc trên bản đồ theo như Tank Trouble gốc)
                        if (activeMyBullets < 5) {
                            bullets.push_back(new Bullet(world, spawnPos, 6.0f * forwardDir, false, false, false, playerIndex));
                            shootCooldownTimer = 0.15f;
                        }
                        break;
                    }
                }
                
                // Trừ đạn cho các vũ khí (Ngoại trừ trường hợp NORMAL hoặc FRAG chưa nổ)
                if (currentWeapon != ItemType::NORMAL && currentWeapon != ItemType::FRAG) {
                    ammo--;
                    if (ammo <= 0) currentWeapon = ItemType::NORMAL;
                }
            }
        }
    }
}

void Tank::CheckCollisions(std::vector<Bullet*>& bullets, std::vector<Item*>& items) {
    for (b2ContactEdge* edge = body->GetContactList(); edge; edge = edge->next) {
        if (edge->contact->IsTouching()) {
            b2Body* otherBody = edge->other;
            
            // Xử lý đạn trúng xe
            for (Bullet* bullet : bullets) {
                if (otherBody == bullet->body) { 
                    bullet->time = 0.0f; 
                    if (hasShield) {
                        hasShield = false; // Vỡ khiên
                        shieldTimer = 0.0f;
                    } else {
                        isDestroyed = true; // Tử nạn
                    }
                }
            }
            
            // Xử lý nhặt vật phẩm
            for (Item* item : items) {
                if (otherBody == item->body && !item->isDestroyed) {
                    item->isDestroyed = true;
                    currentWeapon = item->type;
                    if (currentWeapon == ItemType::GATLING) ammo = 3;
                    else if (currentWeapon == ItemType::FRAG) ammo = 1;
                    else if (currentWeapon == ItemType::MISSILE) ammo = 1;
                    else if (currentWeapon == ItemType::DEATH_RAY) ammo = 1;
                    else ammo = 0;
                }
            }
        }
    }
}

void Tank::Draw() {
    b2Vec2 pos = body->GetPosition();
    float rot = -body->GetAngle() * RAD2DEG;
    float x = pos.x * SCALE, y = SCREEN_HEIGHT - pos.y * SCALE;
    
    if (currentWeapon == ItemType::DEATH_RAY) {
        float currentAngle = body->GetAngle();
        b2Vec2 forwardDir(-sinf(currentAngle), cosf(currentAngle));
        // Vẽ tia laser nhắm mục tiêu (laser sight)
        DrawLineEx({x + forwardDir.x * 30.0f, y - forwardDir.y * 30.0f}, 
                   {x + forwardDir.x * 800.0f, y - forwardDir.y * 800.0f}, 2.0f, ColorAlpha(RED, 0.4f));
    }
    
    Color hullColor = (playerIndex == 0) ? DARKGREEN : (playerIndex == 1) ? DARKBLUE : (playerIndex == 2) ? MAROON : GOLD;
    DrawRectanglePro({ x, y, 6.0f, 14.0f }, { 3.0f, 21.0f }, rot, GRAY);
    DrawRectanglePro({ x, y, 28.0f, 28.0f }, { 14.0f, 7.0f }, rot, hullColor);

    if (hasShield) {
        DrawCircle(x, y, 25.0f, ColorAlpha(SKYBLUE, 0.3f));
        DrawCircleLines(x, y, 25.0f, BLUE);
    }
}
