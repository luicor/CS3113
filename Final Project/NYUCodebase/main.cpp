#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <SDL_mixer.h>
#include "Matrix.h"
#include "ShaderProgram.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <vector>
#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
using namespace std;

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define PI 3.14159265359
#define FIXED_TIMESTEP 0.0166666f
#define TILE_SIZE 1.0f
#define SPRITE_COUNT_X 16
#define SPRITE_COUNT_Y 8
#define mapHeight 25
#define mapWidth 90

SDL_Window* displayWindow;
ShaderProgram program;

void createMap(string input);

GLuint sheet;
GLuint psheet;
GLuint esheet;
GLuint angry;
GLuint fontTexture;
GLuint bg;

Mix_Chunk* jump;
Mix_Chunk* select;
Mix_Music* music1;
Mix_Music* music2;
Mix_Music* music3;
Mix_Music* win;
Mix_Music* lose;
Mix_Music* menu;


vector<int> solids;

const int runAnimation[] = {1, 2, 3, 4};
const int numFrames = 4;
float animationElapsed = 0.0f;
float framesPerSecond = 50.0f;
int currentIndex = 0;

const int moveAnimation[] = {1, 2, 3, 4};
const int eFrames = 4;
int enemyIndex = 0;
float timer = 0.0;


Matrix viewMatrix;
Matrix mapModelMatrix;
Matrix mapMVM;

enum GameMode { STATE_MAIN_MENU, STATE_GAME_OVER, STATE_GAME_LEVEL1, STATE_GAME_LEVEL2, STATE_GAME_LEVEL3, STATE_GAME_WIN, STATE_MANUAL, STATE_PAUSE};

enum EntityType {ENTITY_PLAYER, ENTITY_ENEMY, ENTITY_GOAL};

GameMode mode = STATE_MAIN_MENU;


int levelData[25][90];


float lerp(float v0, float v1, float t) {
    return (1.0 - t)*v0 + t*v1;
}

void worldToTileCoordinates(float worldX, float worldY, int *gridX, int *gridY) {
    *gridX = (int)(worldX / TILE_SIZE);
    *gridY = (int)(-worldY / TILE_SIZE);
}

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
    
    SheetSprite(GLuint tID, int idx) : textureID(tID), index(idx) {
        u = (float)(((int)idx) % SPRITE_COUNT_X) / (float) SPRITE_COUNT_X;
        v = (float)(((int)idx) / SPRITE_COUNT_X) / (float) SPRITE_COUNT_Y;
        width = 1.0/(float)SPRITE_COUNT_X;
        height = 1.0/(float)SPRITE_COUNT_Y;
        size = TILE_SIZE;
    };
    
    SheetSprite(GLuint tID, int idx, string playerID) : textureID(tID), index(idx) {
        u = (float)(((int)idx) % 7) / (float) 7.0;
        v = (float)(((int)idx) / 7) / (float) 5.5;
        width = 1.0/(float)7.0;
        height = 1.0/(float)5.5;
        size = TILE_SIZE;
    };
    
    SheetSprite(GLuint tID, int idx, int enemyMarker) : textureID(tID), index(idx) {
        u = (float)(((int)idx) % 7) / (float) 7.0;
        v = (float)(((int)idx) / 7) / (float) 3.0;
        width = 1.0/(float)7.0;
        height = 1.0/(float)3.0;
        size = TILE_SIZE;
    };
    
    void Draw(ShaderProgram *program);
    
    void DrawUniform(ShaderProgram *program);
    
    int index;
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
    
    glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
    glEnableVertexAttribArray(program->positionAttribute);
    
    glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
    glEnableVertexAttribArray(program->texCoordAttribute);
    
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glDisableVertexAttribArray(program->positionAttribute);
    glDisableVertexAttribArray(program->texCoordAttribute);
}

void SheetSprite::DrawUniform(ShaderProgram *program) {
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

void drawBackground(ShaderProgram* program, GLuint texture)
{
    glBindTexture(GL_TEXTURE_2D, texture);
    float vertices[] = {
        -10.0f, 5.0f,  // Triangle 1 Coord A
        -10.0f, -5.0f, // Triangle 1 Coord B
        10.0f, -5.0f,   // Triangle 1 Coord C
        -10.0f, 5.0f,   // Triangle 2 Coord A
        10.0f, -5.0f, // Triangle 2 Coord B
        10.0f, 5.0f   // Triangle 2 Coord C
    };
    
    
    float texCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 0.0f
    };
    glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
    glEnableVertexAttribArray(program->positionAttribute);
    
    glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
    glEnableVertexAttribArray(program->texCoordAttribute);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    
    glDisableVertexAttribArray(program->positionAttribute);
    glDisableVertexAttribArray(program->texCoordAttribute);
}

class Entity {
public:
    Entity(){
        render = true;
        friction.y = 0.5f;
        friction.x = 1.0f;
        height = TILE_SIZE;
        width = TILE_SIZE * 0.5;
        penetration.x = 0.0;
        penetration.y = 0.0;
    }
    void Draw(ShaderProgram* program) {
        sprite.DrawUniform(program);
    }
    
    //collision with tile handlers
    void collideTileY();
    void collideTileX();
    bool isSolid(int index);
    
    void Update(float elapsed);
    void Render(ShaderProgram &program);
    
    //collision with other entities handler
    void CollidesWith(Entity* entity);
    //supplements collision handler
    bool collision(Entity* entity);
    
    Vector3 position;
    Vector3 velocity;
    Vector3 acceleration;
    Vector3 size;
    Vector3 friction;
    
    Vector3 penetration;
    
    SheetSprite sprite;
    
    Matrix modelMatrix;
    Matrix modelviewMatrix;
    
    bool isStatic;
    EntityType entityType;
    bool render;
    
    float height;
    float width;
    
    bool collidedTop;
    bool collidedBottom;
    bool collidedLeft;
    bool collidedRight;
};

bool Entity::isSolid(int index){
    for(int i = 0; i < solids.size(); ++i){
        if(index == solids[i]){
            return true;
        }
    }
    return false;
}

void Entity::collideTileX(){
    int tileX = 0;
    int tileY = 0;
    
    //left collision
    worldToTileCoordinates(position.x - (width / 2.0f), position.y, &tileX, &tileY);
    
    if(isSolid(levelData[tileY][tileX])) {
        if(entityType == ENTITY_ENEMY){
            velocity.x = 1.0;
            acceleration.x = 2.5;
        }
        collidedLeft = true;
        penetration.x = (position.x - (width / 2)) - (TILE_SIZE * tileX + TILE_SIZE);
        position.x -= (penetration.x - 0.005f);
    }
    else{
        collidedLeft = false;
        penetration.x = 0.0f;
    }
    
    //right collision
    worldToTileCoordinates(position.x + (width / 2), position.y, &tileX, &tileY);
    if(isSolid(levelData[tileY][tileX])) {
        if(entityType == ENTITY_ENEMY){
            velocity.x = -1.0;
            acceleration.x = -2.5;
        }
        collidedRight = true;
        penetration.x = (TILE_SIZE * tileX) - (position.x + (width / 2));
        position.x += (penetration.x - 0.005f);
    }
    else{
        collidedRight = false;
        penetration.x = 0.0f;
    }
    
}

void Entity::collideTileY(){
    int tileX = 0;
    int tileY = 0;
    //top collision
    worldToTileCoordinates(position.x, position.y + (height / 2), &tileX, &tileY);
    if(isSolid(levelData[tileY][tileX])){
        collidedTop = true;
        velocity.y = 0.0f;
        penetration.y = fabs((position.y + (height / 2)) - ((-TILE_SIZE * tileY) - TILE_SIZE));
        position.y -= (penetration.y + 0.005f);
    }
    else{
        collidedTop = false;
        penetration.y = 0.0f;
    }
    
    //bottom collision
    worldToTileCoordinates(position.x, position.y - (height / 2), &tileX, &tileY);
    if(isSolid(levelData[tileY][tileX])) {
        collidedBottom = true;
        velocity.y = 0.0f;
        acceleration.y = 0.0f;
        penetration.y = (-TILE_SIZE * tileY) - (position.y - (height / 2));
        position.y += penetration.y + 0.005f;
    }
    else{
        collidedBottom = false;
        acceleration.y = -5.0f;
    }
}

void Entity::Update(float elapsed) {
    //update velocity of entity
    velocity.x = lerp(velocity.x, 0.0f, elapsed*friction.x);
    velocity.y = lerp(velocity.y, 0.0f, elapsed*friction.y);
    if(entityType == ENTITY_PLAYER) {
        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]) {
            if(mode == STATE_GAME_LEVEL1 || mode == STATE_GAME_LEVEL2 || mode == STATE_GAME_LEVEL3){
                acceleration.x = -3.5f;
                if(collidedBottom == true) {
                    DrawText(&program, fontTexture, "  MOONWALK", 0.5f, 0.0f);
                    sprite = SheetSprite(psheet, runAnimation[currentIndex], "player");
                }
            }
        }
        else if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
            if(mode == STATE_GAME_LEVEL1 || mode == STATE_GAME_LEVEL2 || mode == STATE_GAME_LEVEL3){
                acceleration.x = 3.5f;
                if(collidedBottom == true) {
                    sprite = SheetSprite(psheet, runAnimation[currentIndex], "player");
                }
            }
        }
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) {
            if(mode == STATE_GAME_LEVEL1 || mode == STATE_GAME_LEVEL2 || mode == STATE_GAME_LEVEL3){
                if (collidedBottom == true) {
                    sprite = SheetSprite(psheet, 13, "player");
                    Mix_PlayChannel(-1, jump, 0);
                    velocity.y = 4.8f;
                }
            }
        }
    }
    if(velocity.x < 40.0f){
        velocity.x += acceleration.x * elapsed;
    }
    velocity.y += acceleration.y * elapsed;
    
    //update position and check for collisions with tiles
    position.y += velocity.y * elapsed;
    collideTileY();
    
    position.x += velocity.x * elapsed;
    collideTileX();
    
    if(position.x >= 0.6f) {
        modelMatrix.Identity();
        modelMatrix.Translate(position.x, position.y, 1.0);
    }
}

void Entity::Render(ShaderProgram &program){
    modelviewMatrix = viewMatrix * modelMatrix;
    if(render) {
        this->Draw(&program);
    }
}

bool Entity::collision(Entity* entity) {
    bool collide = false;
    if((position.x + (width / 2) >= entity->position.x - (entity->width / 2)) && (position.x + (width / 2) <= entity->position.x + (entity->width / 2))
       && (position.y - (height / 2) <= entity->position.y + (entity->height / 2))) {
        collide = true;
    }
    else if((position.x - (width / 2) >= entity->position.x - (entity->width / 2)) && (position.x - (width / 2) <= entity->position.x + (entity->width / 2))
            && (position.y - (height / 2) <= entity->position.y + (entity->height / 2))) {
        collide = true;
    }
    return collide;
}

void Entity::CollidesWith(Entity* entity) {
    bool collide = false;
    if(entity->entityType == ENTITY_ENEMY) {
        collide = collision(entity);
    }
    if(entity->entityType == ENTITY_GOAL){
        collide = collision(entity);
    }
    
    if(collide && entity->entityType == ENTITY_GOAL){
        //PLAYER GOES TO NEXT LEVEL
        if(mode == STATE_GAME_LEVEL1) {
            createMap(RESOURCE_FOLDER"level2.txt");
            mode = STATE_GAME_LEVEL2;
            Mix_PlayMusic(music2, -1);
            timer = 0.0;
        }
        else if(mode == STATE_GAME_LEVEL2){
            createMap(RESOURCE_FOLDER"level3.txt");
            mode = STATE_GAME_LEVEL3;
            Mix_PlayMusic(music3, -1);
            timer = 0.0;
        }
        else if(mode == STATE_GAME_LEVEL3) {
            mode = STATE_GAME_WIN;
            Mix_PlayMusic(win, -1);
            timer = 0.0;
        }
    }
    else if(collide && (entity->entityType == ENTITY_ENEMY)){
        //PLAYER DIES GAMEOVER
        mode = STATE_GAME_OVER;
        Mix_PlayMusic(lose, -1);
        timer = 0.0;
    }
    
}

Entity player;
Entity enemy;
Entity goal;

bool readLayerData(ifstream& stream) {
    string line;
    while(getline(stream, line)) {
        if(line == "") {
            break;
        }
        istringstream sStream(line);
        string key,value;
        getline(sStream, key, '=');
        getline(sStream, value);
        
        if(key == "data") {
            for(int y = 0; y < mapHeight; y++) {
                getline(stream, line);
                istringstream lineStream(line);
                string tile;
                for(int x = 0; x < mapWidth; x++) {
                    getline(lineStream, tile, ',');
                    int val =  atoi(tile.c_str());
                    if(val > 0) {
                        // be careful, the tiles in this format are indexed from 1 not 0
                        levelData[y][x] = val-1;
                    } else {
                        levelData[y][x] = 0;
                    }
                }
            } }
    }
    return true;
}
void placeEntity(string type, float x, float y)
{
    if (type == "player") {
        player.entityType = ENTITY_PLAYER;
        player.isStatic = false;
        player.position = Vector3(x, y, 0.0f);
        player.velocity = Vector3(0.0f, 0.0f, 0.0f);
        player.acceleration = Vector3(0.0f, -1.0f, 0.0f);
        player.render = true;
        player.sprite = SheetSprite(psheet, 1, "player");
        player.size.x = player.sprite.width;
        player.size.y = player.sprite.height;
        player.modelMatrix.Identity();
        player.modelMatrix.Translate(x, y, 1.0f);
        
        //then move/create modelviewmatrix and setup view matrix
        viewMatrix.Identity();
        viewMatrix.Translate(-player.position.x, -player.position.y, 1.0f);
        player.modelviewMatrix = viewMatrix * player.modelMatrix;
    }
    else if(type == "enemy"){
        enemy.entityType = ENTITY_ENEMY;
        enemy.isStatic = false;
        enemy.position = Vector3(x, y, 0.0f);
        enemy.velocity = Vector3(2.0f, 0.0f, 0.0f);
        enemy.acceleration = Vector3(1.0f, -1.0f, 0.0f);
        enemy.render = true;
        enemy.sprite = SheetSprite(esheet, 1, 1);
        enemy.size.x = enemy.sprite.width;
        enemy.size.y = enemy.sprite.height;
        enemy.modelMatrix.Identity();
        enemy.modelMatrix.Translate(x, y, 1.0f);
        
        //then move/create modelviewmatrix and setup view matrix
        viewMatrix.Identity();
        viewMatrix.Translate(-player.position.x, -player.position.y, 0.0f);
        enemy.modelviewMatrix = viewMatrix * enemy.modelMatrix;
    }
    else if(type == "goal"){
        goal.entityType = ENTITY_GOAL;
        goal.isStatic = false;
        goal.position = Vector3(x, y, 0.0f);
        goal.velocity = Vector3(0.0f, 0.0f, 0.0f);
        goal.acceleration = Vector3(0.0f, -1.0f, 0.0f);
        goal.render = true;
        goal.sprite = SheetSprite(sheet, 86);
        goal.size.x = enemy.sprite.width;
        goal.size.y = enemy.sprite.height;
        goal.modelMatrix.Identity();
        goal.modelMatrix.Translate(x, y, 1.0f);
        
        //then move/create modelviewmatrix and setup view matrix
        viewMatrix.Identity();
        viewMatrix.Translate(-player.position.x, -player.position.y, 0.0f);
        goal.modelviewMatrix = viewMatrix * goal.modelMatrix;
    }
}

bool readEntityData(ifstream &stream) {
    string line;
    string type;
    while(getline(stream, line)) {
        if(line == "") {
            break;
        }
        
        istringstream sStream(line);
        string key,value;
        getline(sStream, key, '=');
        getline(sStream, value);
        if(key == "type") {
            type = value;
        }
        else if(key == "location") {
            istringstream lineStream(value);
            string xPosition, yPosition;
            getline(lineStream, xPosition, ',');
            getline(lineStream, yPosition, ',');
            float placeX = atoi(xPosition.c_str())*TILE_SIZE;
            float placeY = atoi(yPosition.c_str())*-TILE_SIZE;
            placeEntity(type, placeX, placeY);
        }
    }
    return true;
}

void createMap(string input)
{
    ifstream gamedata(input);
    string line;
    while (getline(gamedata, line)) {
        if (line == "[layer]") {
            readLayerData(gamedata);
        }
        else if (line == "[ObjectsLayer]") {
            readEntityData(gamedata);
        }
    }
}

void drawMap(ShaderProgram* program) {
    glBindTexture(GL_TEXTURE_2D, sheet);
    vector<float> vertexData;
    vector<float> texCoordData;
    for(int y=0; y < mapHeight; y++) {
        for(int x=0; x < mapWidth; x++) {
            if(levelData[y][x] != 0) {
                float u = (float)(((int)levelData[y][x]) % SPRITE_COUNT_X) / (float) SPRITE_COUNT_X;
                float v = (float)(((int)levelData[y][x]) / SPRITE_COUNT_X) / (float) SPRITE_COUNT_Y;
                float spriteWidth = 1.0f/(float)SPRITE_COUNT_X;
                float spriteHeight = 1.0f/(float)SPRITE_COUNT_Y;
                vertexData.insert(vertexData.end(), {
                    TILE_SIZE * x, -TILE_SIZE * y,
                    TILE_SIZE * x, (-TILE_SIZE * y)-TILE_SIZE,
                    (TILE_SIZE * x)+TILE_SIZE, (-TILE_SIZE * y)-TILE_SIZE,
                    TILE_SIZE * x, -TILE_SIZE * y,
                    (TILE_SIZE * x)+TILE_SIZE, (-TILE_SIZE * y)-TILE_SIZE,
                    (TILE_SIZE * x)+TILE_SIZE, -TILE_SIZE * y
                });
                texCoordData.insert(texCoordData.end(), {
                    u, v,
                    u, v+(spriteHeight),
                    u+spriteWidth, v+(spriteHeight),
                    u, v,
                    u+spriteWidth, v+(spriteHeight),
                    u+spriteWidth, v
                });
            }
        }
    }
    
    glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
    glEnableVertexAttribArray(program->positionAttribute);
    
    glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
    glEnableVertexAttribArray(program->texCoordAttribute);
    
    glDrawArrays(GL_TRIANGLES, 0, (int)vertexData.size() / 2);
    
    glDisableVertexAttribArray(program->positionAttribute);
    glDisableVertexAttribArray(program->texCoordAttribute);
}

void Update(float elapsed) {
    if(mode != STATE_PAUSE){
        player.Update(elapsed);
        player.CollidesWith(&enemy);
        player.CollidesWith(&enemy);
        if(abs(enemy.position.x - player.position.x) < 6.0 && abs(enemy.position.y - player.position.y) < 4.0){
            enemy.sprite = SheetSprite(angry, moveAnimation[enemyIndex], 0);
            if(enemy.acceleration.x > 0.0){
                enemy.acceleration.x = 5.0;
            }
            else{
                enemy.acceleration.x = -5.0;
            }
        }
        else{
            enemy.sprite = SheetSprite(esheet, moveAnimation[enemyIndex], 0);
            if(enemy.acceleration.x > 0.0){
                enemy.velocity.x = 1.5;
                enemy.acceleration.x = 2.5;
            }
            else{
                enemy.velocity.x = -1.5;
                enemy.acceleration.x = -2.5;
            }
        }
        //enemy.sprite = SheetSprite(esheet, moveAnimation[enemyIndex], 0);
        enemy.Update(elapsed);
        player.CollidesWith(&goal);
        goal.Update(elapsed);
        viewMatrix.Identity();
        if(player.position.x <= 9.8) {
            viewMatrix.Translate(-9.8 , -player.position.y - 2.0, 0.0f);
        }
        else if(player.position.x >= 80.3) {
            viewMatrix.Translate(-80.3, -player.position.y - 2.0, 0.0f);
        }
        else{
            viewMatrix.Translate(-player.position.x, -player.position.y - 2.0, 0.0f);
        }
    }
    
}

void Render1() {
    mapModelMatrix.Identity();
    mapMVM = viewMatrix * mapModelMatrix;
    program.SetModelviewMatrix(mapMVM);
    drawMap(&program);
    enemy.modelviewMatrix = viewMatrix * enemy.modelMatrix;
    program.SetModelviewMatrix(enemy.modelviewMatrix);
    enemy.Render(program);
    goal.modelviewMatrix = viewMatrix * goal.modelMatrix;
    program.SetModelviewMatrix(goal.modelviewMatrix);
    goal.Render(program);
    player.modelviewMatrix = viewMatrix * player.modelMatrix;
    program.SetModelviewMatrix(player.modelviewMatrix);
    player.Render(program);
}

void Render2() {
    mapModelMatrix.Identity();
    mapMVM = viewMatrix * mapModelMatrix;
    program.SetModelviewMatrix(mapMVM);
    drawMap(&program);
    enemy.modelviewMatrix = viewMatrix * enemy.modelMatrix;
    program.SetModelviewMatrix(enemy.modelviewMatrix);
    enemy.Render(program);
    goal.modelviewMatrix = viewMatrix * goal.modelMatrix;
    program.SetModelviewMatrix(goal.modelviewMatrix);
    goal.Render(program);
    player.modelviewMatrix = viewMatrix * player.modelMatrix;
    program.SetModelviewMatrix(player.modelviewMatrix);
    player.Render(program);
}

void Render3() {
    mapModelMatrix.Identity();
    mapMVM = viewMatrix * mapModelMatrix;
    program.SetModelviewMatrix(mapMVM);
    drawMap(&program);
    enemy.modelviewMatrix = viewMatrix * enemy.modelMatrix;
    program.SetModelviewMatrix(enemy.modelviewMatrix);
    enemy.Render(program);
    goal.modelviewMatrix = viewMatrix * goal.modelMatrix;
    program.SetModelviewMatrix(goal.modelviewMatrix);
    goal.Render(program);
    player.modelviewMatrix = viewMatrix * player.modelMatrix;
    program.SetModelviewMatrix(player.modelviewMatrix);
    player.Render(program);
}

Matrix modelviewMatrix;
Matrix modelviewMatrix2;
Matrix modelviewMatrix3;
Matrix modelviewMatrix4;
Matrix modelviewMatrix5;
Matrix bgMVM;

void RenderSelect(float elapsed){
    switch(mode){
        case STATE_MAIN_MENU:
            bgMVM.Identity();
            program.SetModelviewMatrix(bgMVM);
            drawBackground(&program, bg);
            player.position.x += 0.01;
            if(player.position.x >= 9.90){
                player.position.x = -9.90;
            }
            player.modelviewMatrix.Identity();
            player.modelviewMatrix.Translate(player.position.x, -3.0, 0.0f);
            player.sprite = SheetSprite(psheet, runAnimation[currentIndex], "player");
            program.SetModelviewMatrix(player.modelviewMatrix);
            player.Render(program);
            modelviewMatrix.Identity();
            modelviewMatrix2.Identity();
            modelviewMatrix3.Identity();
            modelviewMatrix.Translate(-4.0, 1.5, 0.0);
            modelviewMatrix2.Translate(-4.6, -1.2, 0.0);
            modelviewMatrix3.Translate(-4.45, -2.2, 0.0);
            program.SetModelviewMatrix(modelviewMatrix);
            DrawText(&program, fontTexture, "Space Boy", 1.0f, 0.0f);
            program.SetModelviewMatrix(modelviewMatrix2);
            DrawText(&program, fontTexture, "Press Space to Begin", 0.5f, 0.0f);
            program.SetModelviewMatrix(modelviewMatrix3);
            DrawText(&program, fontTexture, "Press I to See Instructions", 0.35f, 0.0f);
            timer = 0.0;
            break;
        case STATE_MANUAL:
            bgMVM.Identity();
            program.SetModelviewMatrix(bgMVM);
            drawBackground(&program, bg);
            modelviewMatrix.Identity();
            modelviewMatrix2.Identity();
            modelviewMatrix3.Identity();
            modelviewMatrix4.Identity();
            modelviewMatrix5.Identity();
            modelviewMatrix.Translate(-5.5, 3.0, 0.0);
            modelviewMatrix2.Translate(-9.0, 1.5, 0.0);
            modelviewMatrix3.Translate(0.0, 1.5, 0.0);
            modelviewMatrix4.Translate(-9.0, 0.0, 0.0);
            modelviewMatrix5.Translate(-5.5, -3.0, 0.0);
            program.SetModelviewMatrix(modelviewMatrix);
            DrawText(&program, fontTexture, "INSTRUCTIONS", 1.0f, 0.0f);
            program.SetModelviewMatrix(modelviewMatrix2);
            DrawText(&program, fontTexture, "A or Left - Move Left", 0.35f, 0.0f);
            program.SetModelviewMatrix(modelviewMatrix3);
            DrawText(&program, fontTexture, "D or Right - Move Right", 0.35f, 0.0f);
            program.SetModelviewMatrix(modelviewMatrix4);
            DrawText(&program, fontTexture, "W or Up - Jump", 0.35f, 0.0f);
            program.SetModelviewMatrix(modelviewMatrix5);
            DrawText(&program, fontTexture, "Press Space to Return to Main Menu", 0.35f, 0.0f);
            break;
        case STATE_PAUSE:
            bgMVM.Identity();
            program.SetModelviewMatrix(bgMVM);
            drawBackground(&program, bg);
            modelviewMatrix.Identity();
            modelviewMatrix2.Identity();
            modelviewMatrix3.Identity();
            modelviewMatrix.Translate(-5.15, 0.2, 0.0);
            modelviewMatrix2.Translate(-5.35, -1.2, 0.0);
            modelviewMatrix3.Translate(-4.0, 2.5, 0.0);
            program.SetModelviewMatrix(modelviewMatrix);
            DrawText(&program, fontTexture, "Press Space to Resume", 0.5f, 0.0f);
            program.SetModelviewMatrix(modelviewMatrix2);
            DrawText(&program, fontTexture, "Press Esc to Main Menu", 0.5f, 0.0f);
            program.SetModelviewMatrix(modelviewMatrix3);
            DrawText(&program, fontTexture, "PAUSED", 1.5f, 0.0f);
            break;
        case STATE_GAME_LEVEL1:
            timer += elapsed;
            if(timer < 0.37){
                modelviewMatrix.Identity();
                modelviewMatrix.Translate(-4.0, 1.5, 0.0);
                program.SetModelviewMatrix(modelviewMatrix);
                DrawText(&program, fontTexture, "LEVEL 1", 1.0f, 0.0f);
            }
            Render1();
            break;
        case STATE_GAME_LEVEL2:
            timer += elapsed;
            if(timer < 0.37){
                modelviewMatrix.Identity();
                modelviewMatrix.Translate(-4.0, 0.0, 0.0);
                program.SetModelviewMatrix(modelviewMatrix);
                DrawText(&program, fontTexture, "LEVEL 2", 1.0f, 0.0f);
            }
            Render2();
            break;
        case STATE_GAME_LEVEL3:
            timer += elapsed;
            if(timer < 0.37){
                modelviewMatrix.Identity();
                modelviewMatrix.Translate(-4.0, 1.5, 0.0);
                program.SetModelviewMatrix(modelviewMatrix);
                DrawText(&program, fontTexture, "LEVEL 3", 1.0f, 0.0f);
            }
            Render3();
            break;
        case STATE_GAME_OVER:
            bgMVM.Identity();
            program.SetModelviewMatrix(bgMVM);
            drawBackground(&program, bg);
            modelviewMatrix.Identity();
            modelviewMatrix2.Identity();
            modelviewMatrix.Translate(-4.0, 1.5, 0.0);
            modelviewMatrix2.Translate(-4.7, -1.2, 0.0);
            program.SetModelviewMatrix(modelviewMatrix);
            DrawText(&program, fontTexture, "YOU LOST!", 1.0f, 0.0f);
            program.SetModelviewMatrix(modelviewMatrix2);
            DrawText(&program, fontTexture, "Press Space to Restart", 0.5f, 0.0f);
            break;
        case STATE_GAME_WIN:
            bgMVM.Identity();
            program.SetModelviewMatrix(bgMVM);
            drawBackground(&program, bg);
            modelviewMatrix.Identity();
            modelviewMatrix2.Identity();
            modelviewMatrix.Translate(-4.0, 1.5, 0.0);
            modelviewMatrix2.Translate(-6.9, -1.2, 0.0);
            program.SetModelviewMatrix(modelviewMatrix);
            DrawText(&program, fontTexture, "YOU WON!", 1.0f, 0.0f);
            program.SetModelviewMatrix(modelviewMatrix2);
            DrawText(&program, fontTexture, "Press Space to Go To Main Menu", 0.5f, 0.0f);
            break;
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
    
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);
    jump = Mix_LoadWAV(RESOURCE_FOLDER"jump.wav");
    select = Mix_LoadWAV(RESOURCE_FOLDER"select.wav");
    music1 = Mix_LoadMUS(RESOURCE_FOLDER"music.mp3");
    music2 = Mix_LoadMUS(RESOURCE_FOLDER"cave.mp3");
    music3 = Mix_LoadMUS(RESOURCE_FOLDER"night.mp3");
    win = Mix_LoadMUS(RESOURCE_FOLDER"winner.mp3");
    lose = Mix_LoadMUS(RESOURCE_FOLDER"lose.mp3");
    menu = Mix_LoadMUS(RESOURCE_FOLDER"menu.mp3");
    
    Mix_PlayMusic(menu, -1);
    
    player.position.x = -9.90;
    
    solids = {1, 2, 3, 4, 17, 16, 32, 33, 34};
    
    sheet = LoadTexture(RESOURCE_FOLDER"arne_sprites.png");
    
    psheet = LoadTexture(RESOURCE_FOLDER"p1_spritesheet.png");
    
    angry = LoadTexture(RESOURCE_FOLDER"p3_spritesheet.png");
    
    esheet = LoadTexture(RESOURCE_FOLDER"p2_spritesheet.png");
    
    fontTexture = LoadTexture(RESOURCE_FOLDER"pixel_font.png");
    
    bg =LoadTexture(RESOURCE_FOLDER"starBackground.png");
    
    //Main Menu modelview Matrices
    modelviewMatrix.Identity();
    modelviewMatrix2.Identity();
    modelviewMatrix.Translate(-4.0, 1.5, 0.0);
    modelviewMatrix2.Translate(-4.6, -1.2, 0.0);
    fontTexture = LoadTexture(RESOURCE_FOLDER"pixel_font.png");

    
    ShaderProgram p(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
    program = p;
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glViewport(0, 0, 640, 360);
    Matrix projectionMatrix;
    projectionMatrix.SetOrthoProjection(-9.55f, 9.55f, -4.0f, 4.0f, -1.0f, 1.0f);
    
    
    //Initalize Time Variables
    float lastFrameTicks = 0.0f;
    float accumulator = 0.0f;
    GameMode oldMode = mode;
    
    SDL_Event event;
    bool done = false;
    while (!done) {
        cout << timer << endl;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
                done = true;
            }
            else if (event.type == SDL_KEYDOWN){
                if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE){
                    if(mode != STATE_PAUSE && (mode == STATE_GAME_LEVEL1 || mode == STATE_GAME_LEVEL2 || mode == STATE_GAME_LEVEL3)) {
                        Mix_PlayChannel(-1, select, 0);
                        if(Mix_PlayingMusic() == 1){
                            Mix_PauseMusic();
                        }
                        oldMode = mode;
                        mode = STATE_PAUSE;
                    }
                    else if(mode != STATE_MAIN_MENU && mode == STATE_PAUSE){
                        Mix_PlayChannel(-1, select, 0);
                        mode = STATE_MAIN_MENU;
                        Mix_PlayMusic(menu, -1);
                    }
                    else{
                        done = true;
                    }
                }
                else if(event.key.keysym.scancode == SDL_SCANCODE_SPACE){
                    if(mode == STATE_MAIN_MENU) {
                        createMap(RESOURCE_FOLDER"level1.txt");
                        mode = STATE_GAME_LEVEL1;
                        Mix_PlayChannel(-1, select, 0);
                        Mix_PlayMusic(music1, -1);
                    }
                    else if(mode == STATE_GAME_OVER || mode == STATE_GAME_WIN ||  mode == STATE_MANUAL) {
                        mode = STATE_MAIN_MENU;
                        Mix_PlayChannel(-1, select, 0);
                        Mix_PlayMusic(menu, -1);
                    }
                    else if(mode == STATE_PAUSE){
                        Mix_ResumeMusic();
                        mode = oldMode;
                    }
                }
                else if(event.key.keysym.scancode == SDL_SCANCODE_I){
                    if(mode == STATE_MAIN_MENU){
                        mode = STATE_MANUAL;
                        Mix_PlayChannel(-1, select, 0);
                        Mix_PlayMusic(menu, -1);
                    }
                }
            }
            else if(event.type == SDL_KEYUP){
                if(event.key.keysym.scancode == SDL_SCANCODE_LEFT || event.key.keysym.scancode == SDL_SCANCODE_A){
                    if(mode == STATE_GAME_LEVEL1 || mode == STATE_GAME_LEVEL2 || mode == STATE_GAME_LEVEL3){
                        player.acceleration.x = 0.0f;
                        player.velocity.x = 0.0f;
                    }
                }
                else if(event.key.keysym.scancode == SDL_SCANCODE_RIGHT || event.key.keysym.scancode == SDL_SCANCODE_D){
                    if(mode == STATE_GAME_LEVEL1 || mode == STATE_GAME_LEVEL2 || mode == STATE_GAME_LEVEL3){
                        player.acceleration.x = 0.0f;
                        player.velocity.x = 0.0f;
                    }
                }
            }
        }
        
        glClear(GL_COLOR_BUFFER_BIT);
        
        if(mode == STATE_GAME_LEVEL1) {
            glClearColor(0.0f, 0.5f, 1.0f, 1.0f);
        }
        else if(mode == STATE_GAME_LEVEL3){
            glClearColor(0.3f, 0.0f, 1.0f, 1.0f);
        }
        else{
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }
        
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
            Update(FIXED_TIMESTEP);
            elapsed -= FIXED_TIMESTEP;
            animationElapsed += elapsed;
            if(animationElapsed > 1.0/framesPerSecond){
                currentIndex++;
                enemyIndex++;
                animationElapsed = 0.0;
                
                if(currentIndex > numFrames - 1) {
                    currentIndex = 0;
                }
                if(enemyIndex > eFrames - 1) {
                    enemyIndex = 0;
                }
            }
            
        }
        accumulator = elapsed;
        
        RenderSelect(elapsed);
        
        SDL_GL_SwapWindow(displayWindow);
    }
    
    Mix_FreeChunk(jump);
    Mix_FreeMusic(music1);
    Mix_FreeMusic(music2);
    Mix_FreeMusic(music3);
    Mix_FreeMusic(menu);
    Mix_FreeMusic(lose);
    Mix_FreeMusic(win);
    
    SDL_Quit();
    return 0;
}



