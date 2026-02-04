#include <stdio.h>
#include <math.h>
#include <SDL2/SDL.h>
#include "./constants.h"

int game_is_running = FALSE;

int last_frame_time = 0;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;

typedef struct {
    float x;
    float y;
} Vector2;

typedef struct {
    float x;
    float y;
    float width;
    float height;

    float x_velocity;
    float y_velocity;
} Ball;

typedef struct {
    float x;
    float y;
    float width;
    float height;

    float y_velocity;
} Paddle;

Paddle left_paddle;
Paddle right_paddle;
Ball ball;

int turn;

void setup(void);

int initialize_window(void) {
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
        return FALSE;
    }

    window = SDL_CreateWindow(
        "Game",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0
    );

    if (!window) {
        fprintf(stderr, "Error creating SDL window: %s\n", SDL_GetError());
        return FALSE;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);

    if (!renderer) {
        fprintf(stderr, "Error creating SDL renderer: %s\n", SDL_GetError());
        return FALSE;
    }

    return TRUE;
}

float calculate_angle(float x, float y, float x2, float y2) {
    float dx = x2 - x;
    float dy = y2 - y;

    return atan2f(dy, dx);
}

void setup_ball(void) {
    ball.width = 16;
    ball.height = 16;
    ball.x = (WINDOW_WIDTH / 2) - (ball.width / 2);
    ball.y = (WINDOW_HEIGHT / 2) - (ball.height / 2);
    
    // As it's the first turn, we can pick randomly.
    turn = rand() % 2;

    // Calculate the maximum angle we can fire the ball at based on the window bounds.
    float top_corner_angle;
    float bottom_corner_angle;

    if (turn == LEFT_TURN) {
        printf("Left turn\n");
        top_corner_angle       = calculate_angle(ball.x, ball.y, 0, 0);
        bottom_corner_angle    = calculate_angle(ball.x, ball.y, 0, WINDOW_HEIGHT);
    } else if (turn == RIGHT_TURN) {
        printf("Right turn\n");
        top_corner_angle       = calculate_angle(ball.x, ball.y, WINDOW_WIDTH, 0);
        bottom_corner_angle    = calculate_angle(ball.x, ball.y, WINDOW_WIDTH, WINDOW_HEIGHT);
    }

    const float two_pi = 2.0f * (float)M_PI;
    top_corner_angle = fmodf(top_corner_angle + two_pi, two_pi);
    bottom_corner_angle = fmodf(bottom_corner_angle + two_pi, two_pi);

    float min_angle = fminf(top_corner_angle, bottom_corner_angle);
    float max_angle = fmaxf(top_corner_angle, bottom_corner_angle);

    // If the interval crosses 0 radians, take the shorter wrapped arc.
    if (max_angle - min_angle > (float)M_PI) {
        float tmp = min_angle;
        min_angle = max_angle;
        max_angle = tmp + two_pi;
    }

    float initial_angle = 
        ((float)rand() / (float)RAND_MAX) * (max_angle - min_angle) + min_angle;
    initial_angle = fmodf(initial_angle, two_pi);

    // Calculate the velocity of the ball (between -1, 1) for both x, y, based on the angle
    
    ball.x_velocity = cosf(initial_angle);
    ball.y_velocity = sinf(initial_angle);

    printf("Initial angle: %f\n", initial_angle);
    printf("Initial velocity: %f, %f\n", ball.x_velocity, ball.y_velocity);
}

void setup_left_paddle(void) {
    left_paddle.width = 8;
    left_paddle.height = 64;
    left_paddle.x = 50;
    left_paddle.y = (WINDOW_HEIGHT / 2) - (left_paddle.height / 2);
    left_paddle.y_velocity = 0;
}

void setup_right_paddle(void) {
    right_paddle.width = 8;
    right_paddle.height = 64;
    right_paddle.x = WINDOW_WIDTH - 50 - right_paddle.width;
    right_paddle.y = (WINDOW_HEIGHT / 2) - (right_paddle.height / 2);
    right_paddle.y_velocity = 0;
}

Vector2 calc_ball_pos(float dt, Ball* b) {
    return (Vector2){
        .x = b->x + BALL_SPEED * b->x_velocity * dt,
        .y = b->y + BALL_SPEED * b->y_velocity * dt
    };
}

void update_ball(float dt, Ball* b) {
    // Move the ball
    Vector2 newPos = calc_ball_pos(dt, b);
    int hasCollidedWithPaddle = FALSE;

    // Check if we're colliding with walls, if so, recalculate the velocity.
    // Horizontal Walls
    float angle = atan2(b->y_velocity, b->x_velocity);
    if (newPos.y <= 0 || newPos.y + b->height >= WINDOW_HEIGHT) {
        b->x_velocity = cosf(-angle);
        b->y_velocity = sinf(-angle);

        newPos = calc_ball_pos(dt, b);
    }

    // Check if we're colliding with paddles, if so, recalculate the velocity.

    float ball_top = b->y;
    float ball_center = b->y + b->height / 2;
    float ball_bottom = b->y + b->height;

    float left_paddle_top = left_paddle.y;
    float left_paddle_center = left_paddle.y + left_paddle.height / 2;
    float left_paddle_bottom = left_paddle.y + left_paddle.height;
    float right_paddle_top = right_paddle.y;
    float right_paddle_center = right_paddle.y + right_paddle.height /2;
    float right_paddle_bottom = right_paddle.y + right_paddle.height;

    // Check left paddle
    if (
        (
            (newPos.x <= left_paddle.x + left_paddle.width)     &&  // Ball X is colliding with right bound of left paddle
            (ball_bottom >= left_paddle_top && ball_top <= left_paddle_bottom)      // Ball top is within paddle
        ) || (
            (newPos.x + b->width >= right_paddle.x) &&
            (ball_bottom >= right_paddle_top && ball_top <= right_paddle_bottom)
        )
    ) {
        // find out which paddle it hit
        float bounceAngle = 0.0f;
        if (newPos.x < WINDOW_WIDTH / 2) {// hit left
            float intersect = (left_paddle_center - ball_center) / (left_paddle.height / 2);
            bounceAngle = intersect * ((float)M_PI / MAX_SKEW_ANGLE);
            float min_bounce = (float)M_PI / MIN_SKEW_ANGLE;
            if (fabsf(bounceAngle) < min_bounce)
                bounceAngle = copysignf(min_bounce, bounceAngle);
        } else { // hit right
            float intersect = (right_paddle_center - ball_center) / (right_paddle.height / 2);
            bounceAngle = intersect * ((float)M_PI / MAX_SKEW_ANGLE);
            float min_bounce = (float)M_PI / MIN_SKEW_ANGLE;
            if (fabsf(bounceAngle) < min_bounce)
                bounceAngle = copysignf(min_bounce, bounceAngle);
        }


        b->x_velocity = cosf((float)M_PI - angle - bounceAngle);
        b->y_velocity = sinf((float)M_PI - angle - bounceAngle);

        newPos = calc_ball_pos(dt, b);

        hasCollidedWithPaddle = TRUE;
    }

    if (!hasCollidedWithPaddle) {
        if (newPos.x < GOAL_MARGIN) {
            printf("left goal\n");
            setup_ball();
            return;
        } else if (newPos.x + b->width > WINDOW_WIDTH - GOAL_MARGIN) {
            printf("right goal\n");
            setup_ball();
            return;
        }
    }

    // Check right paddle

    b->x = newPos.x;
    b->y = newPos.y;
}

void update_paddle(float dt, Paddle* paddle) {
    // Is the paddle within bounds?
    float y = paddle->y + paddle->y_velocity * PADDLE_SPEED * dt;

    if (y <= 0) return;
    if (y + paddle->height >= WINDOW_HEIGHT) return;

    paddle->y = y;
}

void setup() {
    setup_ball();
    setup_left_paddle();
    setup_right_paddle();
}

void process_input() {
    SDL_Event event;
    SDL_PollEvent(&event);

    switch (event.type) {
        case SDL_QUIT:
            game_is_running = FALSE;
            break;
        case SDL_KEYDOWN:
            if (event.key.keysym.sym == SDLK_ESCAPE)
                game_is_running = FALSE;
            if (event.key.keysym.sym == SDLK_SPACE)
                setup();

            // Left Paddle Controls
            if (event.key.keysym.sym == SDLK_f) // Down
                left_paddle.y_velocity = PADDLE_DOWN_VELOCITY;
            else if (event.key.keysym.sym == SDLK_d)
                left_paddle.y_velocity = PADDLE_UP_VELOCITY;

            // Right Paddle Controls
            if (event.key.keysym.sym == SDLK_k) // Down
                right_paddle.y_velocity = PADDLE_DOWN_VELOCITY;
            else if (event.key.keysym.sym == SDLK_j)
                right_paddle.y_velocity = PADDLE_UP_VELOCITY;

            break;

        case SDL_KEYUP:
            // Left Paddle Controls
            if (event.key.keysym.sym == SDLK_d || event.key.keysym.sym == SDLK_f)
                left_paddle.y_velocity = PADDLE_STATIONARY_VELOCITY;

            // Right Paddle Controls
            if (event.key.keysym.sym == SDLK_k || event.key.keysym.sym == SDLK_j) // Down
                right_paddle.y_velocity = PADDLE_STATIONARY_VELOCITY;
        default: 
            break;
    }
}

void update(void) {
    
    int time_to_wait = FRAME_TARGET_TIME - (SDL_GetTicks() - last_frame_time);
    if (time_to_wait > 0 && time_to_wait <= FRAME_TARGET_TIME) {
        SDL_Delay(time_to_wait);
    }

    float delta_time = (SDL_GetTicks() - last_frame_time) / 1000.0f;

    last_frame_time = SDL_GetTicks();

    update_ball(delta_time, &ball);
    update_paddle(delta_time, &left_paddle);
    update_paddle(delta_time, &right_paddle);
}

void render() {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_Rect ball_rect = {
        (int) ball.x,
        (int) ball.y,
        (int) ball.width,
        (int) ball.height,
    };

    SDL_Rect left_paddle_rect = {
        (int) left_paddle.x,
        (int) left_paddle.y,
        (int) left_paddle.width,
        (int) left_paddle.height,
    };

    SDL_Rect right_paddle_rect = {
        (int) right_paddle.x,
        (int) right_paddle.y,
        (int) right_paddle.width,
        (int) right_paddle.height,
    };

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    // Draw centre line (vertically centered)
    const float dot_height = (float)WINDOW_HEIGHT / (float)CENTER_LINE_DOTS / 2.0f;
    const float step = (float)WINDOW_HEIGHT / (float)CENTER_LINE_DOTS * 1.25f;
    const float total_height = (CENTER_LINE_DOTS - 1) * step + dot_height;
    const float start_y = (WINDOW_HEIGHT - total_height) / 2.0f;

    for (int i = 0; i < CENTER_LINE_DOTS; i++) {
        SDL_Rect center_dot = {
            (int) WINDOW_WIDTH / 2 - CENTER_LINE_WIDTH / 2,
            (int) (start_y + i * step),
            (int) CENTER_LINE_WIDTH,
            (int) dot_height,
        };

        SDL_RenderFillRect(renderer, &center_dot);
    }

    SDL_RenderFillRect(renderer, &ball_rect);
    SDL_RenderFillRect(renderer, &left_paddle_rect);
    SDL_RenderFillRect(renderer, &right_paddle_rect);

    SDL_RenderPresent(renderer);
}

void destroy_window() {
    printf("Shutting down game");

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main() {
    srand((unsigned)time(NULL));

    game_is_running = initialize_window();

    setup();

    while (game_is_running) {
        process_input();
        update();
        render();
    }

    destroy_window();

    return 0;
}