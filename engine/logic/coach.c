#include "coach.h"
#include "core/constants.h"
#include "entities/ball.h"
#include "entities/team.h"
#include "game/scene.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#define tirak_up (CENTER_Y - (GOAL_WIDTH / 2))
#define tirak_down (CENTER_Y + (GOAL_WIDTH / 2))

bool coach_both_teams = false;

// ==========================================
// بخش ۱: توابع کمکی و ریاضی 
// ==========================================



float get_max_v(struct Player *self){
    //بدست آوردن سرعت ماکس یازیکن
    return self->talents.agility * (MAX_PLAYER_VELOCITY / (MAX_TALENT_PER_SKILL + 0.1f));
}
    //زون بندی زمین
int get_zone_index(float pos, float start, float total_size, int num_zones) {
    if (num_zones <= 0) return 0; 
    int index = (int)((pos - start) / (total_size / (float)num_zones));
    if (index < 0) index = 0;
    if (index >= num_zones) index = num_zones - 1;
    return index;
}
    // محاسبه فاصله تا توپ
float dist_ball(struct Player *self, const struct Scene *scene) {
    float dx = scene->ball->position.x - self->position.x;
    float dy = scene->ball->position.y - self->position.y;
    return sqrt(dx * dx + dy * dy);
}
    // چک کردن مالکیت هم تیمی
bool teammate_has_ball(struct Player *self, const struct Scene *scene) {
    struct Player *owner = scene->ball->possessor;
    return (owner != NULL && owner->team == self->team && owner != self);
}
    //پیدا کردن نزدیک ترین هم تیمی
float mindist(struct Player *self, const struct Scene *scene) {
    float min_dist = 1000000.0f;
    struct Team *my_team = (self->team == 1) ? scene->first_team : scene->second_team;

    for (int i = 0; i < PLAYER_COUNT; i++) {
        struct Player *teammate = my_team->players[i];
        if (!teammate || teammate == self) continue; 
        
        float dx = teammate->position.x - self->position.x;
        float dy = teammate->position.y - self->position.y;
        float dist = sqrt(dx * dx + dy * dy);
        
        if (dist < min_dist) {
            min_dist = dist;
        }
    }
    return min_dist;
}
    //سیستم هوشمند زمان مناسب برای شوت
int should_stop_dribbling(struct Player *self, const struct Scene *scene) {
    // ۱. چک کردن مرزهای زمین (اوت)
    if (self->position.x < PITCH_X || self->position.x > PITCH_X + PITCH_W) {
        return 1; 
    }
    if (self->position.y < PITCH_Y || self->position.y > PITCH_Y + PITCH_H) {
        return 1;
    }

    // ۲. فاصله تا دروازه حریف
    float target_goal_x = (self->team == 1) ? (PITCH_X + PITCH_W) : PITCH_X;
    float dist_to_goal = fabs(target_goal_x - self->position.x);

    if (dist_to_goal < 120.0f) {
        return 1; // منطقه شوت و پاس کلیدی
    }

    // ۳. نزدیک‌ترین بازیکن حریف
    float min_enemy_dist = 10000.0f;
    
    // تشخیص تیم حریف بر اساس ساختار موتور بازی تو
    struct Team *enemy_team = (self->team == 1) ? scene->second_team : scene->first_team;

    for (int i = 0; i < PLAYER_COUNT; i++) {
        struct Player *other = enemy_team->players[i];
        
        // چک کردن برای جلوگیری از سگمنتیشن (اگر بازیکن وجود داشت)
        if (other != NULL) {
            float dx = self->position.x - other->position.x;
            float dy = self->position.y - other->position.y;
            float d = sqrtf(dx * dx + dy * dy);
            
            if (d < min_enemy_dist) {
                min_enemy_dist = d;
            }
        }
    }

    // ۴. فشار حریف
    if (min_enemy_dist < 70.0f) {
        return 1; 
    }

    return 0; // فضا باز است، ادامه حرکت با توپ
}
    //چک کیک آف
bool is_kickoff(const struct Scene *scene) {
    // چک کردن فاصله افقی و عمودی توپ تا نقطه مرکز زمین
    float dx = fabs(scene->ball->position.x - CENTER_X);
    float dy = fabs(scene->ball->position.y - CENTER_Y);

    // اگر هر دو فاصله کمتر از ۵ پیکسل باشن، یعنی توپ در نقطه شروع بازیه
    if (dx < 10.0f && dy < 10.0f) {
        return true;
    }
    
    return false;
}
    //چک شروع کننده
bool is_starter(struct Player *self) {
    // محاسبه فاصله افقی و عمودی بازیکن تا مرکز دقیق زمین
    float dx = fabs(self->position.x - CENTER_X);
    float dy = fabs(self->position.y - CENTER_Y);

    // اگر بازیکن در محدوده ۵ پیکسلی مرکز باشد، یعنی او زننده ضربه شروع است
    if (dx < 45.0f && dy < 45.0f) {
        return true;
    }

    return false;
}


// ==========================================
// بخش ۲: توابع پایه (شوت و حرکت)
// ==========================================


void shoot(struct Player *self, const struct Scene *scene, float target_x, float target_y, float ratio) {
    float dx = target_x - scene->ball->position.x;
    float dy = target_y - scene->ball->position.y;
    float dist = sqrtf(dx * dx + dy * dy) + 0.1f; 

    // حذف متن اضافی و استفاده از نام درست ثابت
    float max_shoot_v = self->talents.shooting * (MAX_BALL_VELOCITY / (MAX_TALENT_PER_SKILL + 0.1f));

    float final_power = max_shoot_v * ratio;

    scene->ball->velocity.x = (dx / dist) * final_power;
    scene->ball->velocity.y = (dy / dist) * final_power;
}
void move_to(struct Player *self, const struct Scene *scene, float target_x, float target_y) {
    (void)scene;
    float dx = target_x - self->position.x;
    float dy = target_y - self->position.y;
    float dist = sqrtf(dx * dx + dy * dy);

    // ۱. توقف نرم و قانونی (فقط صفر کردن سرعت)
    if (dist < 5.0f) {
        self->velocity.x = 0.0f;
        self->velocity.y = 0.0f;
        return;
    }
    // ۲. محاسبه سرعت پایه به سمت هدف
    float max_v = get_max_v(self);
    float vx = (dx / dist) * max_v;
    float vy = (dy / dist) * max_v;
    // ۴. ترمز ایمنی (Clamp): تضمین می‌کند که بردار نهایی از حد مجاز بیشتر نشود
    float final_speed = sqrtf(vx * vx + vy * vy);
    if (final_speed > max_v) {
        self->velocity.x = (vx / final_speed) * max_v;
        self->velocity.y = (vy / final_speed) * max_v;
    } else {
        self->velocity.x = vx;
        self->velocity.y = vy;
    }
}


// ==========================================
// بخش ۳: هوش مصنوعی پاس و شوتینگ
// ==========================================


    //پاس به نزدیک ترین هم تیمی
void pass_to_nearest(struct Player *self, const struct Scene *scene) {
    struct Player *nearest = NULL;
    float min_dist = 1000000.0f;
    struct Team *my_team = (self->team == 1) ? scene->first_team : scene->second_team;
    for (int i = 0; i < PLAYER_COUNT; i++) {
        struct Player *teammate = my_team->players[i];
        if (!teammate || teammate == self) continue;

        float d = dist_ball(teammate, scene);
        if (d < min_dist) {
            min_dist = d;
            nearest = teammate;
        }
    }
    //یا پاس به هم تیمی یا شوت به سمت گل
    float goal_x = (self->team == 1) ? (PITCH_X + PITCH_W) : PITCH_X;
    if (nearest != NULL) {
        if(min_dist < 400.0f && min_dist > 100.0f){
            shoot(self , scene, nearest->position.x, nearest->position.y, 1.0f);
        } else {
            shoot(self , scene, goal_x, CENTER_Y, 1.0f);
        }
    } else {
        shoot(self , scene, goal_x, CENTER_Y, 1.0f);
    }
}
    // سیستم پاس دهی گلر
void gk_smart_pass(struct Player *self, const struct Scene *scene) {
    int choices[] = {0, 1, 2 , 4 , 5}; 
    int random_idx = choices[rand() % 5];

    struct Team *my_team = (self->team == 1) ? scene->first_team : scene->second_team;
    struct Player *target_player = my_team->players[random_idx];

    if (target_player != NULL && target_player != self) {
        shoot(self , scene, target_player->position.x, target_player->position.y, 1.0f);
    } else {
        float goal_x = (self->team == 1) ? (PITCH_X + PITCH_W) : PITCH_X;
        shoot(self , scene, goal_x, CENTER_Y, 1.0f);
    }
}
    // سیستم پاس دهی مدافعین
void df_smart_pass(struct Player *self, const struct Scene *scene) {
    int choices[] = {0, 1 , 5}; 
    int random_idx = choices[rand() % 3];

    struct Team *my_team = (self->team == 1) ? scene->first_team : scene->second_team;
    struct Player *target_player = my_team->players[random_idx];

    if (target_player != NULL && target_player != self) {
        shoot(self , scene, target_player->position.x, target_player->position.y, 1.0f);
    } else {
        float goal_x = (self->team == 1) ? (PITCH_X + PITCH_W) : PITCH_X;
        shoot(self , scene, goal_x, CENTER_Y, 1.0f);
    }
}
    // سیستم پاس مهاجم
void void_kick_cf(struct Player *self, const struct Scene *scene) {
    float target_x , target_y;
    //سیستم شوت زنی رندوم 
    int n = (rand() % 3) + 1;
    if(n == 1) target_y = CENTER_Y + 25.0f;
    if( n == 2) target_y = CENTER_Y;
    if(n == 3) target_y = CENTER_Y - 25.0f;
    
    float shooting_range_x; 
    bool in_shooting_zone = false;

    if (self->team == 1) {
        target_x = PITCH_X + PITCH_W + 50.0f; 
        shooting_range_x = PITCH_X + (PITCH_W * 0.70f); 
        if (scene->ball->position.x >= shooting_range_x) in_shooting_zone = true;
    } else {
        target_x = PITCH_X - 50.0f; 
        shooting_range_x = PITCH_X + (PITCH_W * 0.30f); 
        if (scene->ball->position.x <= shooting_range_x) in_shooting_zone = true;
    }

    if (in_shooting_zone) {
        shoot(self , scene, target_x, target_y, 1.0f); 
    } else {
        pass_to_nearest(self, scene);
    }
}
    // سیستم پاس وینگر
void void_kick_rw(struct Player *self, const struct Scene *scene) {
    //هافبک ها تا وقتی هم تیمی شون نزدیک تر از دروازه حریف باشه پاس مید
    //در غیر این صورت شوت به دروازه میزنن
    float goal_x = (self->team == 1) ? (PITCH_X + PITCH_W) : PITCH_X;
    float dx = goal_x - self->position.x;
    float dy = CENTER_Y - self->position.y;
    float dis_to_goal = sqrt(dx * dx + dy * dy);

    if (dis_to_goal < mindist(self, scene)) {
        void_kick_cf(self, scene); 
    } else {
        pass_to_nearest(self, scene);
    }
}



// ==========================================
// بخش ۴: ماشین وضعیت یکپارچه (State Machine)
// ==========================================
void unified_gk_state(struct Player *self, const struct Scene *scene) {
    float dist = dist_ball(self, scene);

    if (scene->ball->possessor == self){
            self->state = SHOOTING;
        return;
    }
    else if (dist < 26.0f && dist <= mindist(self, scene) && !teammate_has_ball(self, scene)){
        self->state = INTERCEPTING;
        return;
    }

    self->state = MOVING;
}
void unified_change_state(struct Player *self, const struct Scene *scene) {
    float dist = dist_ball(self, scene);
    
    // ۱. اگر من صاحب توپ هستم
    if (scene->ball->possessor == self) {
        if (!should_stop_dribbling(self , scene)) {
            self->state = MOVING;
        }
        else {
            self->state = SHOOTING;
        }
        return;
    }

    else if (dist < 26.0f && dist <= mindist(self, scene) && !teammate_has_ball(self, scene)){
        self->state = INTERCEPTING;
        return;
    }

    self->state = MOVING;
}
void unified_cf_state(struct Player *self, const struct Scene *scene) {
    float dist = dist_ball(self, scene);
    
    // ۱. اگر من صاحب توپ هستم
    if (scene->ball->possessor == self) {
        if (!should_stop_dribbling(self , scene) && !is_kickoff(scene)) {
            self->state = MOVING;
        }
        else {
            self->state = SHOOTING;
        }
        return;
    }

    else if (dist < 26.0f && dist <= mindist(self, scene) && !teammate_has_ball(self, scene)){
        self->state = INTERCEPTING;
        return;
    }

    self->state = MOVING;
}


// ==========================================
// بخش ۵: توابع جایگیری و حرکت در زمین
// ==========================================


void gk_movement(struct Player *self, const struct Scene *scene) {
    float target_x, target_y;
    
    if (self->team == 1) target_x = PITCH_X + 30.0f; 
    else target_x = (PITCH_X + PITCH_W) - 30.0f; 

    //  اگر توپ دست من نیست و دست هم‌تیمی من هم نیست (یعنی یا آزاد است یا دست حریف)
    if (scene->ball->possessor != self && !teammate_has_ball(self, scene) && dist_ball(self, scene) < 45.0f) {
        move_to(self, scene, scene->ball->position.x, scene->ball->position.y);
        return;
    }

    if (scene->ball->position.y < tirak_down) target_y = tirak_down - 30.0f;
    else if (scene->ball->position.y > tirak_up) target_y = tirak_up + 30.0f;
    else target_y = scene->ball->position.y;
    
    move_to(self, scene, target_x, target_y);
}

void df_movement(struct Player *self, const struct Scene *scene) {
    float target_x, target_y;
    float ball_x = scene->ball->position.x;
    float ball_y = scene->ball->position.y;
        //پرس توپ
    if (dist_ball(self, scene) < 80.0f && !teammate_has_ball(self, scene) && scene->ball->possessor != self){
        // قانون طلایی: فقط نزدیک‌ترین مدافع به سمت توپ برود
        if (dist_ball(self, scene) <= mindist(self, scene)) {
            move_to(self, scene, ball_x, ball_y);
            return;
            }
    }

    //بازی بدون توپ (جایگیری دفاعی)
    if (scene->ball->possessor != self){

        int col = get_zone_index(ball_x, PITCH_X, PITCH_W, 3);
        int row = get_zone_index(ball_y, PITCH_Y, PITCH_H, 3);
        if(self->team == 2) col = col - 2 ;
        //دفاع ستونی
        if (col == 0) target_x = PITCH_X + (PITCH_W / 8.0f);      //دفاع خودی
        else if (col == 1) target_x = PITCH_X + (PITCH_W / 4.0f); //وسط
        else target_x = PITCH_X + (PITCH_W / 2.2f);               // در هنگام تهاجم

        if (self->team == 2){
            target_x = PITCH_X + PITCH_W - (target_x - PITCH_X);
        }

        //دغاع ردیفی
        if (row == 0) {
            target_y = (self->kit == 2) ? PITCH_Y + 80.0f : CENTER_Y - 100.0f;
        } else if (row == 2) {
            target_y = (self->kit == 4) ? PITCH_Y + PITCH_H - 100.0f : CENTER_Y + 50.0f;
        } else {
            target_y = (self->kit == 2) ? CENTER_Y - 80.0f : CENTER_Y + 80.0f;
        }
    }
    
    // بازی با توپ: فقط حمله به سمت دروازه حریف
    else{
        // هدف فقط مرکز دروازه مقابل است
        target_y = CENTER_Y; 

        if (self->team == 1) {
            // حرکت به سمت راست - با یک حاشیه امنیت برای بیرون نرفتن
            target_x = (PITCH_X + PITCH_W) - 80.0f;
        } else {
            // حرکت به سمت چپ - با یک حاشیه امنیت برای بیرون نرفتن
            target_x = PITCH_X + 80.0f;
        }
    }
    move_to(self, scene, target_x, target_y);
}

void wing_movement(struct Player *self, const struct Scene *scene) {
    float target_x = self->position.x;
    float target_y = self->position.y;
    float ball_x = scene->ball->position.x;
    float ball_y = scene->ball->position.y;

    if(dist_ball(self, scene) < 75.0f && !teammate_has_ball(self, scene) && scene->ball->possessor != self) {
        move_to(self, scene, ball_x, ball_y);
        return;
    }


    // ۲. بازی بدون توپ (جایگیری زون‌بندی شده)
    if(scene->ball->possessor != self) {
        int col = get_zone_index(ball_x, PITCH_X, PITCH_W, 3);
        
        if (self->team == 1) {
            if (col == 0) { 
                target_x = ball_x + 120.0f; 
                target_y = (self->kit == 1) ? CENTER_Y - 230.0f : CENTER_Y + 230.0f; 
            }
            else if (col == 1) { 
                target_x = ball_x - 40.0f; 
                target_y = (self->kit == 1) ? CENTER_Y - 200.0f : CENTER_Y + 200.0f; 
            }
            else { 
                target_x = ball_x - 60.0f; 
                target_y = (self->kit == 1) ? CENTER_Y - 180.0f : CENTER_Y + 180.0f; 
            }
        } 
        else { // تیم ۲ (آینه‌ای)
            if (col == 2) { 
                target_x = ball_x - 120.0f; 
                target_y = (self->kit == 1) ? CENTER_Y - 230.0f : CENTER_Y + 230.0f; 
            }
            else if (col == 1) { 
                target_x = ball_x + 40.0f; 
                target_y = (self->kit == 1) ? CENTER_Y - 200.0f : CENTER_Y + 200.0f; 
            }
            else { 
                target_x = ball_x + 60.0f; 
                target_y = (self->kit == 1) ? CENTER_Y - 180.0f : CENTER_Y + 180.0f; 
            }
        }
    } 
    // بازی با توپ: فقط حمله به سمت دروازه حریف
    else {
        // هدف فقط مرکز دروازه مقابل است
        target_y = CENTER_Y; 

        if (self->team == 1) {
            target_x = (PITCH_X + PITCH_W) - 80.0f;
        } 
        else{
            target_x = PITCH_X + 80.0f;
        }
    }

    move_to(self, scene, target_x, target_y);
}

void cf_movement(struct Player *self, const struct Scene *scene) {
    float target_x = self->position.x;
    float target_y = self->position.y;
    float ball_x = scene->ball->position.x;
    float ball_y = scene->ball->position.y;

    // ۱. پرس توپ
    if (!teammate_has_ball(self, scene)&& dist_ball(self, scene) < 110.0f && scene->ball->possessor != self && !is_starter(self)) {
        move_to(self, scene, ball_x, ball_y);
        return;
    }

    // ۲. بازی بدون توپ
    if(scene->ball->possessor != self) {
        int col = get_zone_index(ball_x, PITCH_X, PITCH_W, 3);
        int row = get_zone_index(ball_y, PITCH_Y, PITCH_H, 3); 
        if(self->team == 2) col = col - 2 ;
        if(col == 0) target_x = CENTER_X - 100.0f;
        else if(col == 1) target_x = CENTER_X + 100.0f;
        else target_x = CENTER_X + 250.0f; 

        // قرینه‌سازی برای تیم ۲
        if (self->team == 2) target_x = PITCH_X + PITCH_W - (target_x - PITCH_X);

        if(row == 0) target_y = CENTER_Y - 80.0f;
        else if(row == 1) target_y = CENTER_Y;
        else target_y = CENTER_Y + 80.0f; 
    } 
    // بازی با توپ: فقط حمله به سمت دروازه حریف
    else {
        // هدف فقط مرکز دروازه مقابل است
        target_y = CENTER_Y; 

        if (self->team == 1) {
            // حرکت به سمت راست - با یک حاشیه امنیت برای بیرون نرفتن
            target_x = (PITCH_X + PITCH_W) - 80.0f;
        } else {
            // حرکت به سمت چپ - با یک حاشیه امنیت برای بیرون نرفتن
            target_x = PITCH_X + 80.0f;
        }
    }

    move_to(self, scene, target_x, target_y);
}


/* Team 1 movement logic */
void movement_logic_1_0(struct Player *self, const struct Scene *scene) {  cf_movement(self, scene);}//A مهاجم نوک
void movement_logic_1_1(struct Player *self, const struct Scene *scene) { wing_movement(self, scene); }//B وینگر بالا
void movement_logic_1_2(struct Player *self, const struct Scene *scene) {  df_movement(self, scene); }//C دفاع بالا
void movement_logic_1_3(struct Player *self, const struct Scene *scene) {gk_movement(self, scene); } //D دروازه بان
void movement_logic_1_4(struct Player *self, const struct Scene *scene) { df_movement(self, scene);}//E دفاع پایین
void movement_logic_1_5(struct Player *self, const struct Scene *scene) {wing_movement(self, scene); }//F وینگر پایین

/* Team 2 movement logic */
void movement_logic_2_0(struct Player *self, const struct Scene *scene) {  cf_movement(self, scene);}//A مهاجم نوک
void movement_logic_2_1(struct Player *self, const struct Scene *scene) { wing_movement(self, scene); }//B وینگر بالا
void movement_logic_2_2(struct Player *self, const struct Scene *scene) {  df_movement(self, scene); }//C دفاع بالا
void movement_logic_2_3(struct Player *self, const struct Scene *scene) {gk_movement(self, scene); } //D دروازه بان
void movement_logic_2_4(struct Player *self, const struct Scene *scene) { df_movement(self, scene);}//E دفاع پایین
void movement_logic_2_5(struct Player *self, const struct Scene *scene) {wing_movement(self, scene); }//F وینگر پایین

/* Team 1 shooting logic */
void shooting_logic_1_0(struct Player *self, const struct Scene *scene) { void_kick_cf(self, scene); }    // مهاجم نوک (A)
void shooting_logic_1_1(struct Player *self, const struct Scene *scene) { void_kick_rw(self, scene); }    // وینگر بالا (B)
void shooting_logic_1_2(struct Player *self, const struct Scene *scene) { df_smart_pass(self, scene); } // دفاع بالا (C)
void shooting_logic_1_3(struct Player *self, const struct Scene *scene) { gk_smart_pass(self, scene); } // دروازه‌بان (D)
void shooting_logic_1_4(struct Player *self, const struct Scene *scene) { pass_to_nearest(self, scene); } // دفاع پایین (E)
void shooting_logic_1_5(struct Player *self, const struct Scene *scene) { void_kick_rw(self, scene); }    // وینگر پایین (F)

/* Team 2 shooting logic */
void shooting_logic_2_0(struct Player *self, const struct Scene *scene) { void_kick_cf(self, scene); }    // مهاجم نوک (A)
void shooting_logic_2_1(struct Player *self, const struct Scene *scene) { void_kick_rw(self, scene); }    // وینگر بالا (B)
void shooting_logic_2_2(struct Player *self, const struct Scene *scene) { df_smart_pass(self, scene); } // دفاع بالا (C)
void shooting_logic_2_3(struct Player *self, const struct Scene *scene) { gk_smart_pass(self, scene); } // دروازه‌بان (D)
void shooting_logic_2_4(struct Player *self, const struct Scene *scene) { pass_to_nearest(self, scene); } // دفاع پایین (E)
void shooting_logic_2_5(struct Player *self, const struct Scene *scene) { void_kick_rw(self, scene); }    // وینگر پایین (F)

/* Team 1 change_state logic */
void change_state_logic_1_0(struct Player *self, const struct Scene *scene) { unified_cf_state(self, scene); }
void change_state_logic_1_1(struct Player *self, const struct Scene *scene) { unified_change_state(self, scene); }
void change_state_logic_1_2(struct Player *self, const struct Scene *scene) { unified_change_state(self, scene); }
void change_state_logic_1_3(struct Player *self, const struct Scene *scene) { unified_gk_state(self, scene); }
void change_state_logic_1_4(struct Player *self, const struct Scene *scene) { unified_change_state(self, scene); }
void change_state_logic_1_5(struct Player *self, const struct Scene *scene) { unified_change_state(self, scene); }

/* Team 2 change_state logic */
void change_state_logic_2_0(struct Player *self, const struct Scene *scene) { unified_cf_state(self, scene); }
void change_state_logic_2_1(struct Player *self, const struct Scene *scene) { unified_change_state(self, scene); }
void change_state_logic_2_2(struct Player *self, const struct Scene *scene) { unified_change_state(self, scene); }
void change_state_logic_2_3(struct Player *self, const struct Scene *scene) { unified_gk_state(self, scene); }
void change_state_logic_2_4(struct Player *self, const struct Scene *scene) { unified_change_state(self, scene); }
void change_state_logic_2_5(struct Player *self, const struct Scene *scene) { unified_change_state(self, scene); }

/* -------------------------------------------------------------------------
 * Lookup tables for factory
 * ------------------------------------------------------------------------- */
static PlayerLogicFn team1_movement[6] = {
    movement_logic_1_0, movement_logic_1_1, movement_logic_1_2,
    movement_logic_1_3, movement_logic_1_4, movement_logic_1_5
};

static PlayerLogicFn team2_movement[6] = {
    movement_logic_2_0, movement_logic_2_1, movement_logic_2_2,
    movement_logic_2_3, movement_logic_2_4, movement_logic_2_5
};

static PlayerLogicFn team1_shooting[6] = {
    shooting_logic_1_0, shooting_logic_1_1, shooting_logic_1_2,
    shooting_logic_1_3, shooting_logic_1_4, shooting_logic_1_5
};

static PlayerLogicFn team2_shooting[6] = {
    shooting_logic_2_0, shooting_logic_2_1, shooting_logic_2_2,
    shooting_logic_2_3, shooting_logic_2_4, shooting_logic_2_5
};

static PlayerLogicFn team1_change_state[6] = {
    change_state_logic_1_0, change_state_logic_1_1, change_state_logic_1_2,
    change_state_logic_1_3, change_state_logic_1_4, change_state_logic_1_5
};

static PlayerLogicFn team2_change_state[6] = {
    change_state_logic_2_0, change_state_logic_2_1, change_state_logic_2_2,
    change_state_logic_2_3, change_state_logic_2_4, change_state_logic_2_5
};

/* -------------------------------------------------------------------------
 * Factory functions
 * ------------------------------------------------------------------------- */
PlayerLogicFn get_movement_logic(int team, int kit) {
    if (coach_both_teams) return team1_movement[kit];
    return (team == 1) ? team1_movement[kit] : team2_movement[kit];
}

PlayerLogicFn get_shooting_logic(int team, int kit) {
    if (coach_both_teams) return team1_shooting[kit];
    return (team == 1) ? team1_shooting[kit] : team2_shooting[kit];
}

PlayerLogicFn get_change_state_logic(int team, int kit) {
    if (coach_both_teams) return team1_change_state[kit];
    return (team == 1) ? team1_change_state[kit] : team2_change_state[kit];
}

/* -------------------------------------------------------------------------
 * TALENTS
 *  TODO 2: Replace these default values with your desired skill points.
 * ------------------------------------------------------------------------- */
/* Team 1 */
static struct Talents team1_talents[6] = {
    {3, 7, 2, 8}, // مهاجم نوک (A)
    {3, 6, 4, 7}, // وینگر بالا (B)
    {4, 4, 7, 5}, // دفاع بالا (C)
    {1, 4, 6, 9},// دروازه‌بان (D)
    {2, 6, 6, 6}, // دفاع پایین (E)
    {3, 7, 3, 7},  // وینگر پایین (F)
}; // Stamina (استقامت)   Agility (چابکی/سرعت)  Defence (قدرت تدافعی/تکل)  Shooting (قدرت و دقت شوت)


/* Team 2 */
static struct Talents team2_talents[6] = {
    {1, 8, 3, 8},  // مهاجم نوک 
    {2, 5, 6, 7},   // وینگر بالا 
    {3, 4, 7, 6},  // دفاع بالا 
    {4, 3, 5, 8},  // دروازه‌بان 
    {2, 4, 7, 7},  // دفاع پایین 
    {5, 6, 3, 6},   // وینگر پایین 
};

struct Talents get_talents(int team, int kit) {
    if (coach_both_teams) return team1_talents[kit];
    return (team == 1) ? team1_talents[kit] : team2_talents[kit];
}



/* Team 1 */
static struct Vec2 team1_positions[6] = {
    {CENTER_X - 100, CENTER_Y},     // کیت 0: مهاجم نوک )
    {350, CENTER_Y - 120},         // کیت 1: وینگر بالا )
    {200, CENTER_Y - 150},         // کیت 2: مدافع کناری بالا
    {80, CENTER_Y},                // کیت 3: دروازه‌بان 
    {200, CENTER_Y + 150},         // کیت 4: مدافع کناری پایین
    {350, CENTER_Y + 120},         // کیت 5: وینگر پایین
};

/* Team 2*/
static struct Vec2 team2_positions[6] = {
    {CENTER_X + 150 , CENTER_Y},     // کیت 0: مهاجم ()
    {700, CENTER_Y - 180},         // کیت 1: وینگر بالا
    {850, CENTER_Y - 80},          // کیت 2: مدافع مرکزی بالا ()
    {PITCH_X + PITCH_W - 30, CENTER_Y}, // کیت 3: دروازه‌بان
    {850, CENTER_Y + 80},          // کیت 4: مدافع مرکزی پایین
    {700, CENTER_Y + 180},         // کیت 5: وینگر پایین
};

struct Vec2 get_positions(int team, int kit) {
    return (team == 1) ? team1_positions[kit] : team2_positions[kit];
}
