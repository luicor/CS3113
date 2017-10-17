#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include "Matrix.h"
#include "ShaderProgram.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <vector>
#include <string>

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define PI 3.14159265359
#define MAX_BULLETS 30
#define FIXED_TIMESTEP 0.0166666f

SDL_Window* displayWindow;
ShaderProgram Gprogram;

enum GameMode { STATE_MAIN_MENU, STATE_GAME_LEVEL};

GLuint LoadTexture(const char *filePath) {
    int w,h,comp;
    unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);
    if(image == NULL) {
        std::cout << "Unable to load image. Make sure the path is correct\n";
        assert(false);
    }
    GLuint retTexture;
    glGenTextures(1, &retTexture);
    glBindTexture(GL_TEXTURE_2D, retTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    stbi_image_free(image);
    return retTexture;
}

class Vector3 {
public:
    Vector3(){}
    Vector3(float x1, float y1, float z1) : x(x1), y(y1), z(z1) {}
    float x;
    float y;
    float z;
};

class SheetSprite {
public:
    SheetSprite(){}
    SheetSprite(GLuint tID, float uCoord, float vCoord, float w, float h, float s) :
        textureID(tID), u(uCoord), v(vCoord), width(w), height(h), size(s) {};
    
    void Draw(ShaderProgram *program);
    
    float size;
    GLuint textureID;
    float u;
    float v;
    float width;
    float height;
};

void SheetSprite::Draw(ShaderProgram *program) {
    glBindTexture(GL_TEXTURE_2D, textureID);
    GLfloat texCoords[] = {
        u, v+height,
        u+width, v,
        u, v,
        u+width, v,
        u, v+height,
        u+width, v+height
    };
    float aspect = width / height;
    float vertices[] = {
        -0.5f * size * aspect, -0.5f * size,
        0.5f * size * aspect, 0.5f * size,
        -0.5f * size * aspect, 0.5f * size,
        0.5f * size * aspect, 0.5f * size,
        -0.5f * size * aspect, -0.5f * size ,
        0.5f * size * aspect, -0.5f * size
    };
    
    // draw our arrays
    
    glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
    glEnableVertexAttribArray(program->positionAttribute);
    
    glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
    glEnableVertexAttribArray(program->texCoordAttribute);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glDisableVertexAttribArray(program->positionAttribute);
    glDisableVertexAttribArray(program->texCoordAttribute);
}

void DrawText(ShaderProgram *program, int fontTexture, std::string text, float size, float spacing) {
    glBindTexture(GL_TEXTURE_2D, fontTexture);
    float texture_size = 1.0/16.0f;
    std::vector<float> vertexData;
    std::vector<float> texCoordData;
    for(int i=0; i < text.size(); i++) {
        int spriteIndex = (int)text[i];
        float texture_x = (float)(spriteIndex % 16) / 16.0f;
        float texture_y = (float)(spriteIndex / 16) / 16.0f;
        vertexData.insert(vertexData.end(), {
            ((size+spacing) * i) + (-0.5f * size), 0.5f * size,
            ((size+spacing) * i) + (-0.5f * size), -0.5f * size,
            ((size+spacing) * i) + (0.5f * size), 0.5f * size,
            ((size+spacing) * i) + (0.5f * size), -0.5f * size,
            ((size+spacing) * i) + (0.5f * size), 0.5f * size,
            ((size+spacing) * i) + (-0.5f * size), -0.5f * size,
        });
        texCoordData.insert(texCoordData.end(), {
            texture_x, texture_y,
            texture_x, texture_y + texture_size,
            texture_x + texture_size, texture_y,
            texture_x + texture_size, texture_y + texture_size,
            texture_x + texture_size, texture_y,
            texture_x, texture_y + texture_size,
        });
    }
    // draw this data (use the .data() method of std::vector to get pointer to data)
    
    glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
    glEnableVertexAttribArray(program->positionAttribute);
    
    glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
    glEnableVertexAttribArray(program->texCoordAttribute);
    
    glDrawArrays(GL_TRIANGLES, 0, text.length()*6.0f);
    
    glDisableVertexAttribArray(program->positionAttribute);
    glDisableVertexAttribArray(program->texCoordAttribute);
    
}

class Entity {
public:
    Entity(){}
    void Draw(ShaderProgram* program) {
        sprite.Draw(program);
    }
    Vector3 position;
    Vector3 velocity;
    Vector3 size;
    Vector3 direction;
    bool DoA;
    SheetSprite sprite;
};

class GameState {
public:
    GameState(){}
    Entity player;
    Entity enemies[12];
    Entity bullets[MAX_BULLETS];
    int currentDead = 0;
    int score;
};

//KEEP CURRENT GAME MODE AND STATE GLOBALLY
GameMode mode = STATE_MAIN_MENU;
GameState state;

void mainMenu() {
    ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glViewport(0, 0, 640, 360);
    Matrix projectionMatrix;
    projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);

    Matrix modelviewMatrix;
    Matrix modelviewMatrix2;
    modelviewMatrix.Identity();
    modelviewMatrix2.Identity();
    modelviewMatrix.Translate(-3.28, 1.5, 0.0);
    modelviewMatrix2.Translate(-1.95, -1.2, 0.0);
    GLuint fontTexture = LoadTexture(RESOURCE_FOLDER"pixel_font.png");

    SDL_Event event;
    bool done = false;
    while (!done) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
                done = true;
            }
            if(event.type == SDL_KEYDOWN) {
                if(event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                    mode = STATE_GAME_LEVEL;
                    done = true;
                }
            }
        }
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program.programID);
        program.SetProjectionMatrix(projectionMatrix);
        program.SetModelviewMatrix(modelviewMatrix);
        DrawText(&program, fontTexture, "Space Invaders", 0.5f, 0.0f);
        program.SetModelviewMatrix(modelviewMatrix2);
        DrawText(&program, fontTexture, "Press Start to Begin", 0.2f, 0.0f);


        SDL_GL_SwapWindow(displayWindow);
    }
}

void gameLevel() {
    ShaderProgram program(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glViewport(0, 0, 640, 360);
    Matrix projectionMatrix;
    projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);


    Matrix playerModelViewMatrix;
    Matrix scoreModelViewMatrix;
    Matrix enemyModelViewMatrix[12];
    Matrix bulletModelViewMatrix[MAX_BULLETS];

    int bulletIndex = 0;
    int score = 0;
    
    GLuint spriteSheet = LoadTexture(RESOURCE_FOLDER"sheet.png");
    //player setup
    SheetSprite playerTexture(spriteSheet, 310.0f/1024.0f, 907.0f/1024.0f, 98.0f/1024.0f, 75.0f/1024.0f, 0.5f);
    float playerWidth = 0.5 * playerTexture.size * (playerTexture.width / playerTexture.height) * 2;
    float playerHeight = 0.5 * playerTexture.size * 2;
    state.player.sprite = playerTexture;
    playerModelViewMatrix.Translate(0.0, -1.8, 0.0);
    state.player.position = Vector3(0, -1.8, 0.0);
    state.player.size = Vector3(playerWidth, playerHeight, 0.0f);

    //set up font textures
    GLuint fontTexture = LoadTexture(RESOURCE_FOLDER"pixel_font.png");
    scoreModelViewMatrix.Translate(2.90, 1.9, 0.0);
    std::string currentScore = std::to_string(score);
    state.score = score;

    //set up bullets
    SheetSprite bulletTexture(spriteSheet,835.0f/1024.0f, 752.0f/1024.0f, 13.0f/1024.0f, 37.0f/1024.0f, 0.2f);
    float bulletWidth = 0.5f * bulletTexture.size * (bulletTexture.width / bulletTexture.height) * 2;
    float bulletHeight = 0.5f * bulletTexture.size * 2;
    for(int i = 0; i < MAX_BULLETS; ++i) {
        state.bullets[i].position.x = -1000.0f;
        state.bullets[i].position.y = -1000.0f;
        state.bullets[i].sprite = bulletTexture;
        state.bullets[i].size.y = bulletHeight;
        state.bullets[i].size.x = bulletWidth;
        state.bullets[i].velocity = Vector3(0.0, 1.9 , 0.0);
    }
    for(int i = 0; i < MAX_BULLETS; ++i) {
        bulletModelViewMatrix[i].Identity();
        bulletModelViewMatrix[i].SetPosition(0.0, -1000.0, 0.0);
    }

    //set up enemies
    SheetSprite enemyTexture(spriteSheet,425.0f/1024.0f, 384.0f/1024.0f, 93.0f/1024.0f, 84.0f/1024.0f, 0.3f);
    float enemyWidth = 0.5f * enemyTexture.size * (enemyTexture.width / enemyTexture.height) * 2;
    float enemyHeight = 0.5f * enemyTexture.size * 2;
    int initialXPos = -2;
    int initialYPos = 1.7;
    for(int i = 0; i < 6; i++) {
        state.enemies[i].position.x = (initialXPos + i/3.55) * 1.2;
        state.enemies[i].position.y = initialYPos;
        state.enemies[i].sprite = enemyTexture;
        state.enemies[i].size.x = enemyWidth;
        state.enemies[i].size.y = enemyHeight;
        state.enemies[i].velocity = Vector3(1.0, 0.0, 0.0);
        state.enemies[i].direction = Vector3(1.0, 0.0, 0.0);
        state.enemies[i].DoA = true;

    }
    for(int i = 6; i < 12; i++) {
        state.enemies[i].position.x = (initialXPos + (i - 6)/3.55) * 1.2;
        state.enemies[i].position.y = initialYPos - 0.3;
        state.enemies[i].sprite = enemyTexture;
        state.enemies[i].size.x = enemyWidth;
        state.enemies[i].size.y = enemyHeight;
        state.enemies[i].velocity = Vector3(1.0, 0.0, 0.0);
        state.enemies[i].direction = Vector3(1.0, 0.0, 0.0);
        state.enemies[i].DoA = true;
        
    }
    for (int i = 0; i < 6; ++i){
        enemyModelViewMatrix[i].Identity();
        enemyModelViewMatrix[i].Translate((initialXPos + i/3.55)* 1.2, initialYPos, 0);
    }
    for (int i = 6; i < 12; ++i){
        enemyModelViewMatrix[i].Identity();
        enemyModelViewMatrix[i].Translate((initialXPos + (i - 6)/3.55)* 1.2, initialYPos - 0.3, 0);
    }
    
    //Initalize Time Variables
    float lastFrameTicks = 0.0f;
    float accumulator = 0.0f;
    
    
    int lastEnemy = 5;
    int firstEnemy = 0;
    int last2Enemy = 11;
    int first2Enemy = 6;
    
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    SDL_Event event;
    bool done = false;
    while (!done) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
                done = true;
            }
            if(event.type == SDL_KEYDOWN) {
                if(event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                    state.bullets[bulletIndex].position.x = state.player.position.x;
                    state.bullets[bulletIndex].position.y = state.player.position.y;
                    state.bullets[bulletIndex].velocity.y = 1.9;
                    bulletIndex++;
                    if(bulletIndex > MAX_BULLETS - 1) {
                        bulletIndex = 0;
                    }
                }
            }
        }
        
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program.programID);
        program.SetProjectionMatrix(projectionMatrix);

        float ticks = (float)SDL_GetTicks()/1000.0f;
        float elapsed = ticks - lastFrameTicks;
        lastFrameTicks = ticks;
        
        elapsed += accumulator;
        if(elapsed < FIXED_TIMESTEP) {
            accumulator = elapsed;
            continue;
        }
        while(elapsed >= FIXED_TIMESTEP) {
            if(keys[SDL_SCANCODE_A]) {
                if(state.player.position.x > -3.55 + (state.player.size.x/2)) {
                    state.player.position.x -= elapsed * 1.3f;
                    playerModelViewMatrix.Identity();
                    playerModelViewMatrix.Translate(state.player.position.x , -1.8, 0.0);
                }
            }
            else if(keys[SDL_SCANCODE_D]) {
                if(state.player.position.x < 3.55 - (state.player.size.x/2)) {
                    state.player.position.x += elapsed * 1.3f;
                    playerModelViewMatrix.Identity();
                    playerModelViewMatrix.Translate(state.player.position.x , -1.8, 0.0);
                }
            }
            
            //Move bullets that are shot
            for(int i = 0; i < MAX_BULLETS; ++i) {
                state.bullets[i].position.y += elapsed * state.bullets[bulletIndex].velocity.y;
                bulletModelViewMatrix[i].Identity();
                bulletModelViewMatrix[i].Translate(state.bullets[i].position.x, state.bullets[i].position.y, 0.0);
            }
            
            //Move enemies
            //first row of enemies
            if(state.enemies[firstEnemy].position.x < -3.55 + (state.enemies[firstEnemy].size.x/2)) {
                for(int i = firstEnemy; i <= lastEnemy; ++i) {
                    if(state.enemies[i].DoA == true) {
                        state.enemies[i].direction.x *= -1;
                        state.enemies[i].position.x += state.enemies[i].direction.x * elapsed * state.enemies[i].velocity.x;
                        state.enemies[i].position.y -= .05;
                        enemyModelViewMatrix[i].Identity();
                        enemyModelViewMatrix[i].Translate(state.enemies[i].position.x, state.enemies[i].position.y, 0.0);
                    }
                }
            }
            else if(state.enemies[lastEnemy].position.x > 3.55 - (state.enemies[lastEnemy].size.x/2)) {
                for(int i = firstEnemy; i <= lastEnemy; ++i) {
                    if(state.enemies[i].DoA == true) {
                        state.enemies[i].direction.x *= -1;
                        state.enemies[i].position.x += state.enemies[i].direction.x * elapsed * state.enemies[i].velocity.x;
                        state.enemies[i].position.y -= .05;
                        enemyModelViewMatrix[i].Identity();
                        enemyModelViewMatrix[i].Translate(state.enemies[i].position.x, state.enemies[i].position.y, 0.0);
                    }
                }
            }
            else{
                for(int i = firstEnemy; i <= lastEnemy; ++i) {
                    if(state.enemies[i].DoA == true) {
                        state.enemies[i].position.x += state.enemies[i].direction.x * elapsed * state.enemies[i].velocity.x;
                        enemyModelViewMatrix[i].Identity();
                        enemyModelViewMatrix[i].Translate(state.enemies[i].position.x, state.enemies[i].position.y, 0.0);
                    }
                }
            }
            //2nd row of enemies
            if(state.enemies[first2Enemy].position.x < -3.55 + (state.enemies[first2Enemy].size.x/2)) {
                for(int i = first2Enemy; i <= last2Enemy; ++i) {
                    if(state.enemies[i].DoA == true) {
                        state.enemies[i].direction.x *= -1;
                        state.enemies[i].position.x += state.enemies[i].direction.x * elapsed * state.enemies[i].velocity.x;
                        state.enemies[i].position.y -= .05;
                        enemyModelViewMatrix[i].Identity();
                        enemyModelViewMatrix[i].Translate(state.enemies[i].position.x, state.enemies[i].position.y, 0.0);
                    }
                }
            }
            else if(state.enemies[last2Enemy].position.x > 3.55 - (state.enemies[last2Enemy].size.x/2)) {
                for(int i = first2Enemy; i <= last2Enemy; ++i) {
                    if(state.enemies[i].DoA == true) {
                        state.enemies[i].direction.x *= -1;
                        state.enemies[i].position.x += state.enemies[i].direction.x * elapsed * state.enemies[i].velocity.x;
                        state.enemies[i].position.y -= .05;
                        enemyModelViewMatrix[i].Identity();
                        enemyModelViewMatrix[i].Translate(state.enemies[i].position.x, state.enemies[i].position.y, 0.0);
                    }
                }
            }
            else{
                for(int i = first2Enemy; i <= last2Enemy; ++i) {
                    if(state.enemies[i].DoA == true) {
                        state.enemies[i].position.x += state.enemies[i].direction.x * elapsed * state.enemies[i].velocity.x;
                        enemyModelViewMatrix[i].Identity();
                        enemyModelViewMatrix[i].Translate(state.enemies[i].position.x, state.enemies[i].position.y, 0.0);
                    }
                }
            }
            
            
            //Bullet collision handler
            for(int i = 0; i < 12; ++i) {
                for(int j = 0; j < MAX_BULLETS; ++j) {
                    float bulletRight = state.bullets[j].position.x + (state.bullets[j].size.x/2);
                    float bulletLeft = state.bullets[j].position.x - (state.bullets[j].size.x/2);
                    float bulletBottom = state.bullets[j].position.y - (state.bullets[j].size.y/2);
                    float bulletTop = state.bullets[j].position.y + (state.bullets[j].size.y/2);
                    
                    float enemyRight = state.enemies[i].position.x + (state.enemies[i].size.x/2);
                    float enemyLeft = state.enemies[i].position.x - (state.enemies[i].size.x/2);
                    float enemyTop = state.enemies[i].position.y + (state.enemies[i].size.y/2);
                    float enemyBottom = state.enemies[i].position.y - (state.enemies[i].size.y/2);
                    
                    if(enemyTop > bulletBottom && enemyBottom < bulletTop && enemyRight > bulletLeft && enemyLeft < bulletRight) {
                        state.enemies[i].DoA = false;
                        if(i == lastEnemy) {
                            for(int idx = lastEnemy; idx >= firstEnemy; --idx){
                                if(state.enemies[idx].DoA == true) {
                                    lastEnemy = idx;
                                }
                            }
                        }
                        if(i == firstEnemy){
                            for(int idx = firstEnemy; idx <= lastEnemy; ++idx){
                                if(state.enemies[idx].DoA == true) {
                                    firstEnemy = idx;
                                }
                            }
                        }
                        bulletModelViewMatrix[j].Translate(-1000.0, -1000.0, 0.0);
                        enemyModelViewMatrix[i].Translate(-1000.0, -1000.0, 0.0);
                        state.currentDead += 1;
                        state.score += 1;
                    }
                }
            }
            
            float playerRight = state.player.position.x + (state.player.size.x/2);
            float playerLeft = state.player.position.x - (state.player.size.x/2);
            float playerTop = state.player.position.y + (state.player.size.y/2);
            float playerBottom = state.player.position.y - (state.player.size.y/2);
            //enemy to player collision handler
            for(int i = 0; i < 12; ++i) {
                float enemyRight = state.enemies[i].position.x + (state.enemies[i].size.x/2);
                float enemyLeft = state.enemies[i].position.x - (state.enemies[i].size.x/2);
                float enemyTop = state.enemies[i].position.y + (state.enemies[i].size.y/2);
                float enemyBottom = state.enemies[i].position.y - (state.enemies[i].size.y/2);
                
                if(enemyTop > playerBottom && enemyBottom < playerTop && enemyRight > playerLeft && enemyLeft < playerRight){
                    //Player loses goes to main menu
                    mode = STATE_MAIN_MENU;
                    done = true;
                }
            }
            elapsed -= FIXED_TIMESTEP;
        }
        
        accumulator = elapsed;
        
        //Render Player
        program.SetModelviewMatrix(playerModelViewMatrix);
        state.player.sprite.Draw(&program);
    
        //Render Bullets
        for(int i = 0; i < MAX_BULLETS; ++i) {
            program.SetModelviewMatrix(bulletModelViewMatrix[i]);
            state.bullets[i].sprite.Draw(&program);
        }
        //Render Enemies
        for(int i = 0; i < 12; ++i) {
            program.SetModelviewMatrix(enemyModelViewMatrix[i]);
            state.enemies[i].sprite.Draw(&program);
        }
    
        //Render Score
        program.SetModelviewMatrix(scoreModelViewMatrix);
        std::string currentScore = std::to_string(state.score);
        DrawText(&program, fontTexture, currentScore, 0.25, 0.0);
        
        //Reset enemies when all dead
        if(state.currentDead == 12) {
            for(int i = 0; i < 6; i++) {
                state.enemies[i].position.x = (initialXPos + i/3.55) * 1.2;
                state.enemies[i].position.y = initialYPos;
                state.enemies[i].sprite = enemyTexture;
                state.enemies[i].size.x = enemyWidth;
                state.enemies[i].size.y = enemyHeight;
                state.enemies[i].velocity = Vector3(1.0, 0.0, 0.0);
                state.enemies[i].direction = Vector3(1.0, 0.0, 0.0);
                state.enemies[i].DoA = true;
                
            }
            for(int i = 6; i < 12; i++) {
                state.enemies[i].position.x = (initialXPos + (i - 6)/3.55) * 1.2;
                state.enemies[i].position.y = initialYPos - 0.3;
                state.enemies[i].sprite = enemyTexture;
                state.enemies[i].size.x = enemyWidth;
                state.enemies[i].size.y = enemyHeight;
                state.enemies[i].velocity = Vector3(1.0, 0.0, 0.0);
                state.enemies[i].direction = Vector3(1.0, 0.0, 0.0);
                state.enemies[i].DoA = true;
                
            }
            state.currentDead = 0;
        }
        
        
        SDL_GL_SwapWindow(displayWindow);
    }
}

int main(int argc, char *argv[])
{
    SDL_Init(SDL_INIT_VIDEO);
    
    displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
    
    SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
    
    SDL_GL_MakeCurrent(displayWindow, context);
    
    #ifdef _WINDOWS
        glewInit();
    #endif

    bool done = false;
    while(!done) {
        switch(mode) {
            case STATE_MAIN_MENU:
                mainMenu();
                break;
            case STATE_GAME_LEVEL:
                gameLevel();
                done = true;
        }
    }
    SDL_Quit();
    return 0;
}



