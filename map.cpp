#include "map.h"

void GameMap::Build(b2World& world) {
    auto addWall = [&](float x, float y, float width, float height) {
        b2BodyDef def; def.type = b2_staticBody;
        def.position.Set(x / SCALE, (SCREEN_HEIGHT - y) / SCALE); 
        b2PolygonShape shape; shape.SetAsBox((width / 2.0f) / SCALE, (height / 2.0f) / SCALE); 
        b2FixtureDef fixture; fixture.shape = &shape; fixture.density = 0.0f;
        b2Body* body = world.CreateBody(&def);
        body->CreateFixture(&fixture);
        walls.push_back(body);
    };

    std::vector<std::string> maze = {
        "+---+---+---+---+---+---+---+---+", "|               |               |", "+   +---+---+   +   +---+---+   +",
        "|   |       |       |       |   |", "+   +   +   +---+---+   +   +   +", "|       |               |       |",
        "+---+   +               +   +---+", "|       |               |       |", "+   +   +   +---+---+   +   +   +",
        "|   |       |       |       |   |", "+   +---+---+   +   +---+---+   +", "|               |               |",
        "+---+---+---+---+---+---+---+---+"
    };
    float cellW = 90.0f, cellH = 90.0f, wallThickness = 6.0f;
    float offsetX = (SCREEN_WIDTH - (8 * cellW)) / 2.0f, offsetY = (SCREEN_HEIGHT - (6 * cellH)) / 2.0f - 50.0f;

    for (int row = 0; row < maze.size(); row++) {
        for (int col = 0; col < maze[row].length(); col++) {
            char c = maze[row][col];
            if (c == '-' && col % 4 == 2) addWall(offsetX + (col/4)*cellW + cellW/2.0f, offsetY + (row/2)*cellH, cellW + wallThickness, wallThickness);
            else if (c == '|') addWall(offsetX + (col/4)*cellW, offsetY + (row/2)*cellH + cellH/2.0f, wallThickness, cellH + wallThickness);
            else if (c == '+') addWall(offsetX + (col/4)*cellW, offsetY + (row/2)*cellH, wallThickness, wallThickness);
        }
    }
}

void GameMap::Draw() {
    for (b2Body* wall : walls) {
        b2PolygonShape* shape = (b2PolygonShape*)wall->GetFixtureList()->GetShape();
        b2Vec2 pos = wall->GetPosition();
        Rectangle rec = { pos.x * SCALE, SCREEN_HEIGHT - pos.y * SCALE, shape->m_vertices[1].x * 2 * SCALE, shape->m_vertices[2].y * 2 * SCALE };
        DrawRectanglePro(rec, { rec.width / 2.0f, rec.height / 2.0f }, wall->GetAngle() * RAD2DEG, GRAY);
    }
}

void GameMap::Clear(b2World& world) {
    for (b2Body* wall : walls) world.DestroyBody(wall);
    walls.clear();
}