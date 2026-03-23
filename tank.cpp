#include "tank.h"

Tank::Tank(b2World& world, int _playerIndex, int _fw, int _bw, int _tl, int _tr, int _sh) {
    playerIndex = _playerIndex;
    forwardKey = (_fw != 0) ? _fw : KEY_W;
    backwardKey = (_bw != 0) ? _bw : KEY_S;
    turnLeftKey = (_tl != 0) ? _tl : KEY_A;
    turnRightKey = (_tr != 0) ? _tr : KEY_D;
    shootKey = (_sh != 0) ? _sh : KEY_Q;
    shootCooldownTimer = 0.0f;
    isDestroyed = false;

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

void Tank::Move(b2World& world, std::vector<Bullet*>& bullets) {
    float moveSpeed = 2.5f, turnSpeed = 2.5f;
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

    if (shootCooldownTimer > 0.0f) shootCooldownTimer -= GetFrameTime();
    if (IsKeyPressed(shootKey) && shootCooldownTimer <= 0.0f) {
        b2Vec2 startPos = body->GetPosition();
        b2Vec2 spawnPos = startPos + (24.5f / SCALE) * forwardDir;
        class WallRayCastCallback : public b2RayCastCallback {
        public:
            bool hitWall = false;
            float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
                if (fixture->GetBody()->GetType() == b2_staticBody) { hitWall = true; return fraction; }
                return -1.0f;
            }
        };
        WallRayCastCallback callback;
        world.RayCast(&callback, startPos, spawnPos);
        if (!callback.hitWall) {
            bullets.push_back(new Bullet(world, spawnPos, 5.0f * forwardDir));
            shootCooldownTimer = 0.5f;
        }
    }

    for (b2ContactEdge* edge = body->GetContactList(); edge; edge = edge->next) {
        if (edge->contact->IsTouching()) {
            b2Body* otherBody = edge->other;
            for (Bullet* bullet : bullets) {
                if (otherBody == bullet->body) { bullet->time = 0.0f; isDestroyed = true; }
            }
        }
    }
}

void Tank::Draw() {
    b2Vec2 pos = body->GetPosition();
    float rot = -body->GetAngle() * RAD2DEG;
    float x = pos.x * SCALE, y = SCREEN_HEIGHT - pos.y * SCALE;
    
    Color hullColor = (playerIndex == 0) ? DARKGREEN : (playerIndex == 1) ? DARKBLUE : (playerIndex == 2) ? MAROON : GOLD;
    DrawRectanglePro({ x, y, 6.0f, 14.0f }, { 3.0f, 21.0f }, rot, GRAY);
    DrawRectanglePro({ x, y, 28.0f, 28.0f }, { 14.0f, 7.0f }, rot, hullColor);
}
