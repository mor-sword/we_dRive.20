#include <raylib.h>
#include <math.h>
#include <stdbool.h>

#define BACKGROUND_COLOR (Color){186, 149, 127, 255}
#define MAX_SKIDMARKS 500
#define SKIDMARK_TIME 3.0f
/* Grid and camera view settings */
#define GRID_SCALE 2.0f
#define CAMERA_BASE_ZOOM 0.80f
#define CAMERA_SPEED_ZOOM_OUT 0.10f

/* Straight infinite-road settings */
#define ROAD_CENTER_X 0.0f
#define ROAD_WIDTH 900.0f
#define ROAD_SHOULDER_WIDTH 70.0f
#define ROAD_COLLISION_MARGIN 55.0f

#define ROAD_EDGE_LINE_WIDTH 12.0f
#define ROAD_CENTER_LINE_WIDTH 9.0f
#define ROAD_DASH_LENGTH 130.0f
#define ROAD_DASH_GAP 95.0f

/* Obstacle and health-pickup settings */
#define MAX_OBSTACLES 16
#define MAX_HEALTH_PICKUPS 5
#define OBSTACLE_DAMAGE 25.0f
#define HEALTH_PICKUP_AMOUNT 30.0f
#define HIT_INVULNERABILITY_TIME 1.0f
#define OBJECT_MIN_SPAWN_AHEAD 900
#define OBJECT_MAX_SPAWN_AHEAD 4300

typedef struct
{
    Vector2 left_tire;
    Vector2 right_tire;
    double time;
} Skidmark;

typedef struct
{
    Vector2 position;
    float radius;
    int type;
} Obstacle;

typedef struct
{
    Vector2 position;
    float radius;
} HealthPickup;

float ClampFloat(float value, float minimum, float maximum)
{
    if (value < minimum)
        return minimum;

    if (value > maximum)
        return maximum;

    return value;
}

float ShortestAngleDifference(float target, float current)
{
    float difference = fmodf(target - current + 180.0f, 360.0f);

    if (difference < 0.0f)
        difference += 360.0f;

    return difference - 180.0f;
}

Image LoadSoilImage(void)
{
    Image image = LoadImage("img/Soil_Tile.png");

    if (image.data == NULL)
        image = LoadImage("img/soil_tile.png");

    if (image.data == NULL)
        image = LoadImage("img/Soil_Tile.png.png");

    if (image.data == NULL)
    {
        image = GenImageChecked(
            256,
            256,
            32,
            32,
            (Color){135, 100, 80, 255},
            (Color){120, 88, 70, 255});
    }

    return image;
}

Image LoadCarImage(void)
{
    Image image = LoadImage("img/Car_1_01.png");

    if (image.data == NULL)
        image = LoadImage("img/car_1_01.png");

    if (image.data == NULL)
        image = LoadImage("img/Car_1_01.png.png");

    if (image.data == NULL)
    {
        image = GenImageColor(100, 180, BLANK);

        ImageDrawRectangle(&image, 5, 20, 15, 35, BLACK);
        ImageDrawRectangle(&image, 80, 20, 15, 35, BLACK);
        ImageDrawRectangle(&image, 5, 125, 15, 35, BLACK);
        ImageDrawRectangle(&image, 80, 125, 15, 35, BLACK);

        ImageDrawRectangle(&image, 15, 10, 70, 160, RED);

        ImageDrawRectangle(&image, 20, 50, 60, 70, DARKBLUE);
        ImageDrawRectangle(&image, 25, 55, 50, 60, SKYBLUE);

        ImageDrawRectangle(&image, 25, 10, 12, 8, YELLOW);
        ImageDrawRectangle(&image, 63, 10, 12, 8, YELLOW);
    }

    return image;
}

void DrawFuelGauge(float fuel, float maximum_fuel)
{
    const int x = 30;
    const int y = 75;
    const int width = 260;
    const int height = 25;

    float ratio = ClampFloat(fuel / maximum_fuel, 0.0f, 1.0f);

    Color fuel_color = GREEN;

    if (ratio <= 0.25f)
        fuel_color = RED;
    else if (ratio <= 0.50f)
        fuel_color = ORANGE;

    DrawText("FUEL", x, y - 27, 20, WHITE);

    DrawRectangle(
        x + 4,
        y + 5,
        width,
        height,
        (Color){0, 0, 0, 100});

    DrawRectangle(
        x,
        y,
        width,
        height,
        (Color){25, 28, 35, 230});

    DrawRectangle(
        x + 3,
        y + 3,
        (int)((width - 6) * ratio),
        height - 6,
        fuel_color);

    DrawRectangleLines(x, y, width, height, WHITE);

    const char *fuel_text = TextFormat("%.0f%%", ratio * 100.0f);
    int text_width = MeasureText(fuel_text, 18);

    DrawText(
        fuel_text,
        x + width / 2 - text_width / 2,
        y + 3,
        18,
        WHITE);

    DrawText("Hold R to refuel", x, y + 35, 17, LIGHTGRAY);
}

void DrawHealthGauge(float health, float maximum_health)
{
    const int x = 30;
    const int y = 165;
    const int width = 260;
    const int height = 25;

    float ratio = ClampFloat(
        health / maximum_health,
        0.0f,
        1.0f);

    Color health_color = GREEN;

    if (ratio <= 0.25f)
        health_color = RED;
    else if (ratio <= 0.50f)
        health_color = ORANGE;

    DrawText("CAR HEALTH", x, y - 27, 20, WHITE);

    DrawRectangle(
        x + 4,
        y + 5,
        width,
        height,
        (Color){0, 0, 0, 100});

    DrawRectangle(
        x,
        y,
        width,
        height,
        (Color){25, 28, 35, 230});

    DrawRectangle(
        x + 3,
        y + 3,
        (int)((width - 6) * ratio),
        height - 6,
        health_color);

    DrawRectangleLines(x, y, width, height, WHITE);

    const char *health_text = TextFormat(
        "%.0f%%",
        ratio * 100.0f);

    int text_width = MeasureText(health_text, 18);

    DrawText(
        health_text,
        x + width / 2 - text_width / 2,
        y + 3,
        18,
        WHITE);

    DrawText(
        "Collect green health kits to repair",
        x,
        y + 35,
        17,
        LIGHTGRAY);
}

void DrawSpeedometer(
    float car_speed,
    float maximum_car_speed,
    int screen_width,
    int screen_height)
{
    const float speed_to_kmh = 15.0f;
    const float radius = 110.0f;
    const float start_angle = 135.0f;
    const float total_angle = 270.0f;

    float speed_kmh = fabsf(car_speed) * speed_to_kmh;
    float maximum_kmh = maximum_car_speed * speed_to_kmh;

    float ratio = ClampFloat(
        speed_kmh / maximum_kmh,
        0.0f,
        1.0f);

    Vector2 center = {
        (float)screen_width - 150.0f,
        (float)screen_height - 150.0f};

    DrawCircle(
        (int)center.x + 6,
        (int)center.y + 8,
        radius + 10.0f,
        (Color){0, 0, 0, 100});

    DrawCircle(
        (int)center.x,
        (int)center.y,
        radius + 8.0f,
        (Color){12, 15, 20, 235});

    DrawCircle(
        (int)center.x,
        (int)center.y,
        radius - 6.0f,
        (Color){25, 29, 36, 245});

    DrawRing(
        center,
        radius - 5.0f,
        radius,
        start_angle,
        start_angle + total_angle,
        80,
        LIGHTGRAY);

    DrawRing(
        center,
        radius - 15.0f,
        radius - 8.0f,
        start_angle,
        start_angle + total_angle,
        80,
        (Color){55, 60, 70, 255});

    Color speed_color = SKYBLUE;

    if (ratio >= 0.85f)
        speed_color = RED;
    else if (ratio >= 0.65f)
        speed_color = ORANGE;

    if (ratio > 0.0f)
    {
        DrawRing(
            center,
            radius - 15.0f,
            radius - 8.0f,
            start_angle,
            start_angle + total_angle * ratio,
            80,
            speed_color);
    }

    for (int i = 0; i <= 10; i++)
    {
        float tick_ratio = (float)i / 10.0f;
        float tick_angle = start_angle + total_angle * tick_ratio;
        float radians = tick_angle * DEG2RAD;

        Vector2 outer_point = {
            center.x + cosf(radians) * (radius - 20.0f),
            center.y + sinf(radians) * (radius - 20.0f)};

        Vector2 inner_point = {
            center.x + cosf(radians) * (radius - 35.0f),
            center.y + sinf(radians) * (radius - 35.0f)};

        DrawLineEx(inner_point, outer_point, 3.0f, WHITE);

        int tick_speed = (int)(maximum_kmh * tick_ratio);
        const char *tick_text = TextFormat("%d", tick_speed);
        int tick_width = MeasureText(tick_text, 13);

        DrawText(
            tick_text,
            (int)(center.x +
                  cosf(radians) * (radius - 52.0f) -
                  tick_width / 2.0f),
            (int)(center.y +
                  sinf(radians) * (radius - 52.0f) -
                  6.0f),
            13,
            LIGHTGRAY);
    }

    for (int i = 0; i <= 40; i++)
    {
        if (i % 4 == 0)
            continue;

        float tick_ratio = (float)i / 40.0f;
        float tick_angle = start_angle + total_angle * tick_ratio;
        float radians = tick_angle * DEG2RAD;

        Vector2 outer_point = {
            center.x + cosf(radians) * (radius - 21.0f),
            center.y + sinf(radians) * (radius - 21.0f)};

        Vector2 inner_point = {
            center.x + cosf(radians) * (radius - 28.0f),
            center.y + sinf(radians) * (radius - 28.0f)};

        DrawLineEx(inner_point, outer_point, 1.5f, GRAY);
    }

    float needle_angle = start_angle + total_angle * ratio;
    float needle_radians = needle_angle * DEG2RAD;

    Vector2 needle_end = {
        center.x + cosf(needle_radians) * (radius - 42.0f),
        center.y + sinf(needle_radians) * (radius - 42.0f)};

    DrawLineEx(
        (Vector2){center.x + 3.0f, center.y + 3.0f},
        (Vector2){needle_end.x + 3.0f, needle_end.y + 3.0f},
        6.0f,
        (Color){0, 0, 0, 130});

    DrawLineEx(center, needle_end, 5.0f, RED);

    DrawCircle((int)center.x, (int)center.y, 11.0f, RED);
    DrawCircle((int)center.x, (int)center.y, 5.0f, WHITE);

    const char *speed_text = TextFormat("%.0f", speed_kmh);
    int speed_width = MeasureText(speed_text, 32);

    DrawText(
        speed_text,
        (int)center.x - speed_width / 2,
        (int)center.y + 34,
        32,
        WHITE);

    const char *unit_text = "km/h";
    int unit_width = MeasureText(unit_text, 16);

    DrawText(
        unit_text,
        (int)center.x - unit_width / 2,
        (int)center.y + 67,
        16,
        LIGHTGRAY);

    const char *gear_text = "N";
    Color gear_color = GREEN;

    if (car_speed > 0.1f)
    {
        gear_text = "D";
        gear_color = SKYBLUE;
    }
    else if (car_speed < -0.1f)
    {
        gear_text = "R";
        gear_color = ORANGE;
    }

    int gear_width = MeasureText(gear_text, 26);

    DrawText(
        gear_text,
        (int)center.x - gear_width / 2,
        (int)center.y - 66,
        26,
        gear_color);
}

void DrawInfiniteSoilGrid(
    Texture2D soil_texture,
    Camera2D camera,
    int screen_width,
    int screen_height)
{
    Vector2 top_left = GetScreenToWorld2D(
        (Vector2){0.0f, 0.0f},
        camera);

    Vector2 top_right = GetScreenToWorld2D(
        (Vector2){(float)screen_width, 0.0f},
        camera);

    Vector2 bottom_left = GetScreenToWorld2D(
        (Vector2){0.0f, (float)screen_height},
        camera);

    Vector2 bottom_right = GetScreenToWorld2D(
        (Vector2){(float)screen_width, (float)screen_height},
        camera);

    float minimum_x = fminf(
        fminf(top_left.x, top_right.x),
        fminf(bottom_left.x, bottom_right.x));

    float maximum_x = fmaxf(
        fmaxf(top_left.x, top_right.x),
        fmaxf(bottom_left.x, bottom_right.x));

    float minimum_y = fminf(
        fminf(top_left.y, top_right.y),
        fminf(bottom_left.y, bottom_right.y));

    float maximum_y = fmaxf(
        fmaxf(top_left.y, top_right.y),
        fmaxf(bottom_left.y, bottom_right.y));

    /*
       GRID_SCALE changes the actual visible dimensions
       of every repeated soil tile.
    */
    float tile_width =
        (float)soil_texture.width * GRID_SCALE;

    float tile_height =
        (float)soil_texture.height * GRID_SCALE;

    int first_tile_x =
        (int)floorf(minimum_x / tile_width) - 2;

    int last_tile_x =
        (int)ceilf(maximum_x / tile_width) + 2;

    int first_tile_y =
        (int)floorf(minimum_y / tile_height) - 2;

    int last_tile_y =
        (int)ceilf(maximum_y / tile_height) + 2;

    Rectangle source = {
        0.0f,
        0.0f,
        (float)soil_texture.width,
        (float)soil_texture.height};

    for (int tile_x = first_tile_x;
         tile_x <= last_tile_x;
         tile_x++)
    {
        for (int tile_y = first_tile_y;
             tile_y <= last_tile_y;
             tile_y++)
        {
            Rectangle destination = {
                (float)tile_x * tile_width,
                (float)tile_y * tile_height,
                tile_width + 1.0f,
                tile_height + 1.0f};

            DrawTexturePro(
                soil_texture,
                source,
                destination,
                (Vector2){0.0f, 0.0f},
                0.0f,
                WHITE);
        }
    }
}

void DrawInfiniteStraightRoad(
    Camera2D camera,
    int screen_width,
    int screen_height)
{
    Vector2 top_left = GetScreenToWorld2D(
        (Vector2){0.0f, 0.0f},
        camera);

    Vector2 top_right = GetScreenToWorld2D(
        (Vector2){(float)screen_width, 0.0f},
        camera);

    Vector2 bottom_left = GetScreenToWorld2D(
        (Vector2){0.0f, (float)screen_height},
        camera);

    Vector2 bottom_right = GetScreenToWorld2D(
        (Vector2){
            (float)screen_width,
            (float)screen_height},
        camera);

    float minimum_y = fminf(
        fminf(top_left.y, top_right.y),
        fminf(bottom_left.y, bottom_right.y));

    float maximum_y = fmaxf(
        fmaxf(top_left.y, top_right.y),
        fmaxf(bottom_left.y, bottom_right.y));

    /*
       Extra visible space prevents empty road edges
       while the camera moves.
    */
    minimum_y -= 400.0f;
    maximum_y += 400.0f;

    float road_left =
        ROAD_CENTER_X -
        ROAD_WIDTH / 2.0f;

    float road_right =
        ROAD_CENTER_X +
        ROAD_WIDTH / 2.0f;

    float shoulder_left =
        road_left -
        ROAD_SHOULDER_WIDTH;

    float shoulder_right =
        road_right +
        ROAD_SHOULDER_WIDTH;

    float visible_height =
        maximum_y -
        minimum_y;

    Color shoulder_color = {
        102,
        86,
        70,
        255};

    Color road_color = {
        52,
        55,
        60,
        255};

    /*
       Draw the two shoulders first.
    */
    DrawRectangleRec(
        (Rectangle){
            shoulder_left,
            minimum_y,
            ROAD_SHOULDER_WIDTH,
            visible_height},
        shoulder_color);

    DrawRectangleRec(
        (Rectangle){
            road_right,
            minimum_y,
            ROAD_SHOULDER_WIDTH,
            visible_height},
        shoulder_color);

    /*
       Draw the asphalt road.
    */
    DrawRectangleRec(
        (Rectangle){
            road_left,
            minimum_y,
            ROAD_WIDTH,
            visible_height},
        road_color);

    /*
       White lines along both edges.
    */
    DrawRectangleRec(
        (Rectangle){
            road_left +
                ROAD_EDGE_LINE_WIDTH,
            minimum_y,
            ROAD_EDGE_LINE_WIDTH,
            visible_height},
        RAYWHITE);

    DrawRectangleRec(
        (Rectangle){
            road_right -
                ROAD_EDGE_LINE_WIDTH * 2.0f,
            minimum_y,
            ROAD_EDGE_LINE_WIDTH,
            visible_height},
        RAYWHITE);

    /*
       Red-and-white shoulder blocks provide a clearer
       visual boundary.
    */
    const float barrier_block_height = 110.0f;

    int first_barrier_block =
        (int)floorf(
            minimum_y /
            barrier_block_height) -
        1;

    int last_barrier_block =
        (int)ceilf(
            maximum_y /
            barrier_block_height) +
        1;

    for (
        int block = first_barrier_block;
        block <= last_barrier_block;
        block++)
    {
        float block_y =
            (float)block *
            barrier_block_height;

        Color block_color =
            (block % 2 == 0)
                ? RED
                : RAYWHITE;

        DrawRectangleRec(
            (Rectangle){
                shoulder_left,
                block_y,
                ROAD_SHOULDER_WIDTH,
                barrier_block_height},
            block_color);

        DrawRectangleRec(
            (Rectangle){
                road_right,
                block_y,
                ROAD_SHOULDER_WIDTH,
                barrier_block_height},
            block_color);
    }

    /*
       Infinite dashed centre line.
    */
    float dash_cycle =
        ROAD_DASH_LENGTH +
        ROAD_DASH_GAP;

    int first_dash =
        (int)floorf(
            minimum_y /
            dash_cycle) -
        1;

    int last_dash =
        (int)ceilf(
            maximum_y /
            dash_cycle) +
        1;

    for (
        int dash = first_dash;
        dash <= last_dash;
        dash++)
    {
        float dash_y =
            (float)dash *
            dash_cycle;

        DrawRectangleRec(
            (Rectangle){
                ROAD_CENTER_X -
                    ROAD_CENTER_LINE_WIDTH / 2.0f,
                dash_y,
                ROAD_CENTER_LINE_WIDTH,
                ROAD_DASH_LENGTH},
            (Color){245, 210, 65, 255});
    }
}

float RandomRoadX(float edge_margin)
{
    float minimum_x =
        ROAD_CENTER_X -
        ROAD_WIDTH / 2.0f +
        edge_margin;

    float maximum_x =
        ROAD_CENTER_X +
        ROAD_WIDTH / 2.0f -
        edge_margin;

    return (float)GetRandomValue(
        (int)minimum_x,
        (int)maximum_x);
}

void PlaceObstacle(Obstacle *obstacle, float y)
{
    obstacle->position = (Vector2){
        RandomRoadX(105.0f),
        y};

    obstacle->radius =
        (float)GetRandomValue(34, 48);

    obstacle->type =
        GetRandomValue(0, 2);
}

void RespawnObstacleAhead(
    Obstacle *obstacle,
    float car_y,
    int order)
{
    float distance =
        (float)GetRandomValue(
            OBJECT_MIN_SPAWN_AHEAD,
            OBJECT_MAX_SPAWN_AHEAD);

    distance += (float)order * 55.0f;

    PlaceObstacle(
        obstacle,
        car_y - distance);
}

void PlaceHealthPickup(
    HealthPickup *pickup,
    float y)
{
    pickup->position = (Vector2){
        RandomRoadX(115.0f),
        y};

    pickup->radius = 34.0f;
}

void RespawnHealthPickupAhead(
    HealthPickup *pickup,
    float car_y,
    int order)
{
    float distance =
        (float)GetRandomValue(1400, 4800);

    distance += (float)order * 180.0f;

    PlaceHealthPickup(
        pickup,
        car_y - distance);
}

void ResetRoadObjects(
    Obstacle obstacles[MAX_OBSTACLES],
    HealthPickup health_pickups[MAX_HEALTH_PICKUPS],
    float car_y)
{
    for (int i = 0; i < MAX_OBSTACLES; i++)
    {
        float y =
            car_y -
            750.0f -
            (float)i * 260.0f -
            (float)GetRandomValue(0, 170);

        PlaceObstacle(&obstacles[i], y);
    }

    for (int i = 0; i < MAX_HEALTH_PICKUPS; i++)
    {
        float y =
            car_y -
            1300.0f -
            (float)i * 920.0f -
            (float)GetRandomValue(0, 300);

        PlaceHealthPickup(
            &health_pickups[i],
            y);
    }
}

bool CarTouchesCircle(
    float car_x,
    float car_y,
    float car_angle,
    float car_length,
    Vector2 circle_center,
    float circle_radius)
{
    float radians =
        (car_angle - 90.0f) *
        DEG2RAD;

    Vector2 forward = {
        cosf(radians),
        sinf(radians)};

    Vector2 front_point = {
        car_x + forward.x * car_length * 0.25f,
        car_y + forward.y * car_length * 0.25f};

    Vector2 rear_point = {
        car_x - forward.x * car_length * 0.25f,
        car_y - forward.y * car_length * 0.25f};

    const float car_collision_radius = 34.0f;

    return CheckCollisionCircles(
               front_point,
               car_collision_radius,
               circle_center,
               circle_radius) ||
           CheckCollisionCircles(
               rear_point,
               car_collision_radius,
               circle_center,
               circle_radius);
}

void DrawObstacle(Obstacle obstacle)
{
    Vector2 shadow = {
        obstacle.position.x + 8.0f,
        obstacle.position.y + 10.0f};

    DrawCircleV(
        shadow,
        obstacle.radius + 3.0f,
        (Color){0, 0, 0, 85});

    if (obstacle.type == 0)
    {
        Rectangle crate = {
            obstacle.position.x - obstacle.radius,
            obstacle.position.y - obstacle.radius,
            obstacle.radius * 2.0f,
            obstacle.radius * 2.0f};

        DrawRectangleRec(
            crate,
            (Color){128, 78, 38, 255});

        DrawRectangleLinesEx(
            crate,
            5.0f,
            (Color){75, 43, 24, 255});

        DrawLineEx(
            (Vector2){crate.x, crate.y},
            (Vector2){
                crate.x + crate.width,
                crate.y + crate.height},
            5.0f,
            (Color){75, 43, 24, 255});

        DrawLineEx(
            (Vector2){
                crate.x + crate.width,
                crate.y},
            (Vector2){
                crate.x,
                crate.y + crate.height},
            5.0f,
            (Color){75, 43, 24, 255});
    }
    else if (obstacle.type == 1)
    {
        DrawCircleV(
            obstacle.position,
            obstacle.radius,
            (Color){35, 35, 38, 255});

        DrawCircleV(
            obstacle.position,
            obstacle.radius * 0.58f,
            (Color){80, 82, 88, 255});

        DrawCircleV(
            obstacle.position,
            obstacle.radius * 0.27f,
            (Color){25, 25, 28, 255});

        DrawCircleLines(
            (int)obstacle.position.x,
            (int)obstacle.position.y,
            obstacle.radius,
            BLACK);
    }
    else
    {
        float radius = obstacle.radius;

        Vector2 top = {
            obstacle.position.x,
            obstacle.position.y - radius};

        Vector2 bottom_left = {
            obstacle.position.x - radius * 0.75f,
            obstacle.position.y + radius};

        Vector2 bottom_right = {
            obstacle.position.x + radius * 0.75f,
            obstacle.position.y + radius};

        DrawTriangle(
            top,
            bottom_left,
            bottom_right,
            ORANGE);

        DrawRectangle(
            (int)(obstacle.position.x - radius * 0.52f),
            (int)(obstacle.position.y + radius * 0.10f),
            (int)(radius * 1.04f),
            (int)(radius * 0.28f),
            RAYWHITE);

        DrawRectangle(
            (int)(obstacle.position.x - radius * 0.95f),
            (int)(obstacle.position.y + radius * 0.82f),
            (int)(radius * 1.90f),
            (int)(radius * 0.28f),
            DARKGRAY);
    }
}

void DrawHealthPickup(HealthPickup pickup)
{
    float pulse =
        1.0f +
        0.08f *
            sinf(
                (float)GetTime() * 4.0f +
                pickup.position.y * 0.01f);

    float radius = pickup.radius * pulse;

    DrawCircleV(
        (Vector2){
            pickup.position.x + 7.0f,
            pickup.position.y + 9.0f},
        radius + 4.0f,
        (Color){0, 0, 0, 80});

    DrawCircleV(
        pickup.position,
        radius,
        (Color){20, 120, 58, 255});

    DrawCircleV(
        pickup.position,
        radius - 5.0f,
        (Color){45, 205, 90, 255});

    DrawRectangle(
        (int)(pickup.position.x - radius * 0.18f),
        (int)(pickup.position.y - radius * 0.62f),
        (int)(radius * 0.36f),
        (int)(radius * 1.24f),
        WHITE);

    DrawRectangle(
        (int)(pickup.position.x - radius * 0.62f),
        (int)(pickup.position.y - radius * 0.18f),
        (int)(radius * 1.24f),
        (int)(radius * 0.36f),
        WHITE);

    DrawCircleLines(
        (int)pickup.position.x,
        (int)pickup.position.y,
        radius,
        RAYWHITE);
}

int main(void)
{
    const int screen_width = 1700;
    const int screen_height = 1000;

    InitWindow(
        screen_width,
        screen_height,
        "Dustline Drift");

    SetTargetFPS(120);
    ChangeDirectory(GetApplicationDirectory());

    Image soil_image = LoadSoilImage();
    ImageRotateCW(&soil_image);

    Texture2D soil_texture =
        LoadTextureFromImage(soil_image);

    SetTextureFilter(
        soil_texture,
        TEXTURE_FILTER_BILINEAR);

    UnloadImage(soil_image);

    Image car_image = LoadCarImage();

    /* Remove transparent space around the car sprite. */
    ImageAlphaCrop(&car_image, 0.1f);

    Texture2D car_texture =
        LoadTextureFromImage(car_image);

    GenTextureMipmaps(&car_texture);

    SetTextureFilter(
        car_texture,
        TEXTURE_FILTER_BILINEAR);

    UnloadImage(car_image);

    Rectangle car_texture_source = {
        0.0f,
        0.0f,
        (float)car_texture.width,
        (float)car_texture.height};

    const float car_width = 90.0f;
    const float car_length = 160.0f;

    /*
       The road continues infinitely along the Y-axis.
       The car begins in the middle of the road.
    */
    float car_x = ROAD_CENTER_X;
    float car_y = 0.0f;
    float car_speed = 0.0f;

    const float car_max_speed = 12.0f;
    const float car_acceleration = 15.0f;
    const float car_friction = 2.5f;

    float car_angle = 0.0f;
    float drift_angle = 0.0f;
    float steering = 0.0f;

    const float steering_speed = 120.0f;
    const float maximum_steering = 60.0f;
    const float steering_return_speed = 4.0f;

    float fuel = 100.0f;

    const float maximum_fuel = 100.0f;
    const float fuel_consumption_rate = 1.0f;
    const float refuel_rate = 25.0f;

    float health = 100.0f;
    const float maximum_health = 100.0f;
    double last_obstacle_hit_time = -10.0;

    Obstacle obstacles[MAX_OBSTACLES] = {0};
    HealthPickup health_pickups[MAX_HEALTH_PICKUPS] = {0};

    ResetRoadObjects(
        obstacles,
        health_pickups,
        car_y);

    Skidmark skidmarks[MAX_SKIDMARKS] = {0};
    int skidmark_count = 0;

    Camera2D camera = {
        (Vector2){
            (float)screen_width / 2.0f,
            (float)screen_height * 0.68f},
        (Vector2){car_x, car_y},
        0.0f,
        CAMERA_BASE_ZOOM};

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        if (dt > 0.05f)
            dt = 0.05f;

        if (
            health <= 0.0f &&
            IsKeyPressed(KEY_ENTER))
        {
            car_x = ROAD_CENTER_X;
            car_y = 0.0f;
            car_speed = 0.0f;
            car_angle = 0.0f;
            drift_angle = 0.0f;
            steering = 0.0f;
            fuel = maximum_fuel;
            health = maximum_health;
            last_obstacle_hit_time = -10.0;
            skidmark_count = 0;

            ResetRoadObjects(
                obstacles,
                health_pickups,
                car_y);

            camera.target = (Vector2){
                car_x,
                car_y};
        }

        bool car_is_alive = health > 0.0f;

        bool forward_pressed =
            car_is_alive &&
            (IsKeyDown(KEY_W) ||
             IsKeyDown(KEY_UP));

        bool reverse_pressed =
            car_is_alive &&
            (IsKeyDown(KEY_S) ||
             IsKeyDown(KEY_DOWN));

        if (
            car_is_alive &&
            IsKeyDown(KEY_R))
        {
            fuel += refuel_rate * dt;

            if (fuel > maximum_fuel)
                fuel = maximum_fuel;
        }

        bool engine_has_fuel =
            fuel > 0.0f &&
            car_is_alive;

        if (forward_pressed && engine_has_fuel)
        {
            car_speed += car_acceleration * dt;

            if (car_speed > car_max_speed)
                car_speed = car_max_speed;

            float fuel_speed_ratio =
                fabsf(car_speed) / car_max_speed;

            fuel -=
                fuel_consumption_rate *
                (0.35f + 0.65f * fuel_speed_ratio) *
                dt;
        }
        else if (reverse_pressed && engine_has_fuel)
        {
            car_speed -= car_acceleration * dt;

            if (car_speed < -car_max_speed / 2.0f)
                car_speed = -car_max_speed / 2.0f;

            fuel -=
                fuel_consumption_rate *
                0.70f *
                dt;
        }
        else
        {
            if (car_speed > 0.0f)
            {
                car_speed -= car_friction * dt;

                if (car_speed < 0.0f)
                    car_speed = 0.0f;
            }
            else if (car_speed < 0.0f)
            {
                car_speed += car_friction * dt;

                if (car_speed > 0.0f)
                    car_speed = 0.0f;
            }

            if (fabsf(car_speed) < 0.1f)
                car_speed = 0.0f;
        }

        if (fuel < 0.0f)
            fuel = 0.0f;

        if (
            car_is_alive &&
            (IsKeyDown(KEY_A) ||
             IsKeyDown(KEY_LEFT)))
        {
            steering -= steering_speed * dt;

            if (steering < -maximum_steering)
                steering = -maximum_steering;
        }
        else if (
            car_is_alive &&
            (IsKeyDown(KEY_D) ||
             IsKeyDown(KEY_RIGHT)))
        {
            steering += steering_speed * dt;

            if (steering > maximum_steering)
                steering = maximum_steering;
        }
        else
        {
            if (steering > 0.0f)
            {
                steering -=
                    steering_speed *
                    steering_return_speed *
                    dt;

                if (steering < 0.0f)
                    steering = 0.0f;
            }
            else if (steering < 0.0f)
            {
                steering +=
                    steering_speed *
                    steering_return_speed *
                    dt;

                if (steering > 0.0f)
                    steering = 0.0f;
            }
        }

        car_angle +=
            steering *
            (car_speed / car_max_speed) *
            dt *
            5.0f;

        car_angle = fmodf(car_angle, 360.0f);

        float drift_follow_difference =
            ShortestAngleDifference(
                car_angle,
                drift_angle);

        const float drift_follow_speed = 4.0f;

        drift_angle +=
            drift_follow_difference *
            drift_follow_speed *
            dt;

        float drift_difference =
            ShortestAngleDifference(
                drift_angle,
                car_angle);

        bool drifting =
            fabsf(drift_difference) > 1.5f &&
            fabsf(car_speed) >
                car_max_speed * 0.4f;

        float car_radians =
            (car_angle - 90.0f) *
            DEG2RAD;

        float drift_radians =
            (drift_angle - 90.0f) *
            DEG2RAD;

        car_x +=
            car_speed *
            cosf(car_radians) *
            dt *
            60.0f;

        car_y +=
            car_speed *
            sinf(car_radians) *
            dt *
            60.0f;

        car_x +=
            car_speed *
            cosf(drift_radians) *
            dt *
            20.0f;

        car_y +=
            car_speed *
            sinf(drift_radians) *
            dt *
            20.0f;

        /*
           Keep the car inside the straight road.
           The road has no limit along the Y-axis.
        */
        float left_road_limit =
            ROAD_CENTER_X -
            ROAD_WIDTH / 2.0f +
            ROAD_COLLISION_MARGIN;

        float right_road_limit =
            ROAD_CENTER_X +
            ROAD_WIDTH / 2.0f -
            ROAD_COLLISION_MARGIN;

        if (car_x < left_road_limit)
        {
            car_x = left_road_limit;
            car_speed *= 0.35f;
            steering *= 0.40f;
            drift_angle = car_angle;
        }
        else if (car_x > right_road_limit)
        {
            car_x = right_road_limit;
            car_speed *= 0.35f;
            steering *= 0.40f;
            drift_angle = car_angle;
        }

        double gameplay_time = GetTime();

        for (int i = 0; i < MAX_OBSTACLES; i++)
        {
            bool obstacle_is_behind =
                obstacles[i].position.y >
                car_y + 950.0f;

            bool obstacle_is_too_far =
                obstacles[i].position.y <
                car_y - 5400.0f;

            if (
                obstacle_is_behind ||
                obstacle_is_too_far)
            {
                RespawnObstacleAhead(
                    &obstacles[i],
                    car_y,
                    i);
            }

            bool can_take_damage =
                health > 0.0f &&
                gameplay_time -
                        last_obstacle_hit_time >=
                    HIT_INVULNERABILITY_TIME;

            if (
                can_take_damage &&
                CarTouchesCircle(
                    car_x,
                    car_y,
                    car_angle,
                    car_length,
                    obstacles[i].position,
                    obstacles[i].radius))
            {
                health -= OBSTACLE_DAMAGE;

                if (health < 0.0f)
                    health = 0.0f;

                last_obstacle_hit_time =
                    gameplay_time;

                car_speed *= -0.22f;
                steering *= 0.25f;
                drift_angle = car_angle;

                RespawnObstacleAhead(
                    &obstacles[i],
                    car_y,
                    i);
            }
        }

        for (int i = 0; i < MAX_HEALTH_PICKUPS; i++)
        {
            bool pickup_is_behind =
                health_pickups[i].position.y >
                car_y + 950.0f;

            bool pickup_is_too_far =
                health_pickups[i].position.y <
                car_y - 5600.0f;

            if (
                pickup_is_behind ||
                pickup_is_too_far)
            {
                RespawnHealthPickupAhead(
                    &health_pickups[i],
                    car_y,
                    i);
            }

            if (
                health > 0.0f &&
                health < maximum_health &&
                CarTouchesCircle(
                    car_x,
                    car_y,
                    car_angle,
                    car_length,
                    health_pickups[i].position,
                    health_pickups[i].radius))
            {
                health += HEALTH_PICKUP_AMOUNT;

                if (health > maximum_health)
                    health = maximum_health;

                RespawnHealthPickupAhead(
                    &health_pickups[i],
                    car_y,
                    i);
            }
        }

        if (health <= 0.0f)
        {
            car_speed = 0.0f;
            steering = 0.0f;
        }

        float speed_ratio = ClampFloat(
            fabsf(car_speed) / car_max_speed,
            0.0f,
            1.0f);

        float camera_direction =
            (drift_angle - 90.0f) *
            DEG2RAD;

        float look_ahead_distance =
            180.0f * speed_ratio;

        Vector2 desired_camera_target = {
            car_x +
                cosf(camera_direction) *
                    look_ahead_distance,

            car_y +
                sinf(camera_direction) *
                    look_ahead_distance};

        const float camera_follow_speed = 4.0f;

        camera.target.x +=
            (desired_camera_target.x -
             camera.target.x) *
            camera_follow_speed *
            dt;

        camera.target.y +=
            (desired_camera_target.y -
             camera.target.y) *
            camera_follow_speed *
            dt;

        /*
           Keep the camera upright so the infinite road
           always remains straight on the display.
        */
        float desired_camera_rotation =
            0.0f;

        float camera_rotation_difference =
            ShortestAngleDifference(
                desired_camera_rotation,
                camera.rotation);

        const float camera_rotation_speed = 2.2f;

        camera.rotation +=
            camera_rotation_difference *
            camera_rotation_speed *
            dt;

        camera.rotation =
            fmodf(camera.rotation, 360.0f);

        /*
           CAMERA_BASE_ZOOM changes the actual view.
           Lower values display more of the world.
        */
        float desired_zoom =
            CAMERA_BASE_ZOOM -
            CAMERA_SPEED_ZOOM_OUT *
                speed_ratio;

        const float camera_zoom_speed = 2.0f;

        camera.zoom +=
            (desired_zoom - camera.zoom) *
            camera_zoom_speed *
            dt;

        if (drifting)
        {
            float left_tire_angle =
                (car_angle - 150.0f) *
                DEG2RAD;

            float right_tire_angle =
                (car_angle - 30.0f) *
                DEG2RAD;

            float tire_distance =
                car_length / 3.0f;

            int index =
                skidmark_count %
                MAX_SKIDMARKS;

            skidmarks[index].left_tire = (Vector2){
                car_x +
                    tire_distance *
                        cosf(left_tire_angle),

                car_y +
                    tire_distance *
                        sinf(left_tire_angle)};

            skidmarks[index].right_tire = (Vector2){
                car_x +
                    tire_distance *
                        cosf(right_tire_angle),

                car_y +
                    tire_distance *
                        sinf(right_tire_angle)};

            skidmarks[index].time = GetTime();

            skidmark_count++;
        }

        BeginDrawing();
        ClearBackground(BACKGROUND_COLOR);

        BeginMode2D(camera);

        DrawInfiniteSoilGrid(
            soil_texture,
            camera,
            screen_width,
            screen_height);

        /*
           Draw the infinite asphalt road above the soil.
        */
        DrawInfiniteStraightRoad(
            camera,
            screen_width,
            screen_height);

        double current_time = GetTime();

        int stored_skidmark_count = skidmark_count;

        if (stored_skidmark_count > MAX_SKIDMARKS)
            stored_skidmark_count = MAX_SKIDMARKS;

        for (int i = 0; i < stored_skidmark_count; i++)
        {
            double age =
                current_time -
                skidmarks[i].time;

            if (age > SKIDMARK_TIME)
                continue;

            float visibility =
                1.0f -
                (float)(age / SKIDMARK_TIME);

            unsigned char alpha =
                (unsigned char)(190.0f *
                                visibility);

            Color skidmark_color = {
                40,
                40,
                40,
                alpha};

            DrawCircleV(
                skidmarks[i].left_tire,
                4.0f,
                skidmark_color);

            DrawCircleV(
                skidmarks[i].right_tire,
                4.0f,
                skidmark_color);
        }

        for (int i = 0; i < MAX_OBSTACLES; i++)
        {
            DrawObstacle(obstacles[i]);
        }

        for (int i = 0; i < MAX_HEALTH_PICKUPS; i++)
        {
            DrawHealthPickup(health_pickups[i]);
        }

        Rectangle car_destination = {
            car_x,
            car_y,
            car_width,
            car_length};

        Rectangle shadow_destination = {
            car_x + 7.0f,
            car_y + 10.0f,
            car_width,
            car_length};

        Vector2 car_origin = {
            car_width / 2.0f,
            car_length / 2.0f};

        DrawTexturePro(
            car_texture,
            car_texture_source,
            shadow_destination,
            car_origin,
            car_angle,
            (Color){0, 0, 0, 75});

        Color car_tint = WHITE;

        bool recently_hit =
            GetTime() - last_obstacle_hit_time <
            HIT_INVULNERABILITY_TIME;

        if (
            recently_hit &&
            ((int)(GetTime() * 12.0) % 2) == 0)
        {
            car_tint = (Color){255, 110, 110, 255};
        }

        DrawTexturePro(
            car_texture,
            car_texture_source,
            car_destination,
            car_origin,
            car_angle,
            car_tint);

        EndMode2D();

        DrawText(
            "WASD / Arrow Keys: Drive",
            30,
            20,
            20,
            WHITE);

        DrawFuelGauge(
            fuel,
            maximum_fuel);

        DrawHealthGauge(
            health,
            maximum_health);

        DrawSpeedometer(
            car_speed,
            car_max_speed,
            screen_width,
            screen_height);

        if (
            fuel > 0.0f &&
            fuel <= maximum_fuel * 0.20f)
        {
            bool show_warning =
                ((int)(GetTime() * 3.0) % 2) == 0;

            if (show_warning)
            {
                const char *warning =
                    "LOW FUEL";

                int warning_width =
                    MeasureText(warning, 30);

                DrawText(
                    warning,
                    screen_width / 2 -
                        warning_width / 2,
                    40,
                    30,
                    ORANGE);
            }
        }

        if (
            fuel <= 0.0f &&
            health > 0.0f)
        {
            const char *message =
                "OUT OF FUEL - HOLD R TO REFUEL";

            int message_width =
                MeasureText(message, 30);

            DrawRectangle(
                screen_width / 2 -
                    message_width / 2 -
                    20,
                245,
                message_width + 40,
                55,
                (Color){0, 0, 0, 180});

            DrawText(
                message,
                screen_width / 2 -
                    message_width / 2,
                257,
                30,
                RED);
        }

        if (
            health > 0.0f &&
            health <= maximum_health * 0.25f)
        {
            bool show_health_warning =
                ((int)(GetTime() * 4.0) % 2) == 0;

            if (show_health_warning)
            {
                const char *warning =
                    "CRITICAL CAR DAMAGE";

                int warning_width =
                    MeasureText(warning, 28);

                DrawText(
                    warning,
                    screen_width / 2 -
                        warning_width / 2,
                    82,
                    28,
                    RED);
            }
        }

        if (health <= 0.0f)
        {
            DrawRectangle(
                0,
                0,
                screen_width,
                screen_height,
                (Color){0, 0, 0, 145});

            const char *wrecked = "CAR WRECKED";
            const char *restart =
                "Press ENTER to restart";

            int wrecked_width =
                MeasureText(wrecked, 54);

            int restart_width =
                MeasureText(restart, 28);

            DrawText(
                wrecked,
                screen_width / 2 -
                    wrecked_width / 2,
                screen_height / 2 - 60,
                54,
                RED);

            DrawText(
                restart,
                screen_width / 2 -
                    restart_width / 2,
                screen_height / 2 + 12,
                28,
                WHITE);
        }

        DrawFPS(
            screen_width - 120,
            30);

        EndDrawing();
    }

    UnloadTexture(car_texture);
    UnloadTexture(soil_texture);

    CloseWindow();

    return 0;
}
