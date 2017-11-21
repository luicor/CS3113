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
#define mapHeight 23
#define mapWidth 58

SDL_Window* displayWindow;
ShaderProgram program;

Mix_Chunk* jump;
Mix_Chunk* coinSound;
Mix_Music* music;

GLuint sheet;

vector<int> solids;

Matrix viewMatrix;
Matrix mapModelMatrix;
Matrix mapMVM;

enum GameMode { STATE_MAIN_MENU, STATE_GAME_LEVEL};

enum EntityType {ENTITY_PLAYER, ENTITY_COIN};

int levelData[23][58];


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
        collidedLeft = true;
        velocity.x = 0.0f;
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
        collidedRight = true;
        velocity.x = 0.0f;
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
            acceleration.x = -2.5f;
        }
        else if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) {
            acceleration.x = 2.5f;
        }
        if (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) {
            if (collidedBottom == true) {
                Mix_PlayChannel(-1, jump, 0);
                velocity.y = 4.0f;
            }
        }
    }
    
    if(velocity.x < 20.0f){
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
    if(entity->entityType == ENTITY_COIN) {
        collide = collision(entity);
    }
    
    if(collide){
        Mix_PlayChannel(-1, coinSound, 0);
        entity->render = false;
        entity->modelMatrix.Identity();
        entity->modelMatrix.Translate(-2000.0f, -2000.0f, 1.0f);
        entity->modelviewMatrix = viewMatrix * entity->modelMatrix;
    }
    
}

Entity player;
Entity coin;

//bool readHeader(ifstream& stream) {
//    string line;
//    mapWidth = -1;
//    mapHeight = -1;
//    while(getline(stream, line)) {
//        if(line == "") {
//            break;
//        }
//
//        istringstream sStream(line);
//        string key,value;
//        getline(sStream, key, '=');
//        getline(sStream, value);
//
//        if(key == "width") {
//            mapWidth = atoi(value.c_str());
//        }
//        else if(key == "height"){
//            mapHeight = atoi(value.c_str());
//        }
//    }
//    if(mapWidth == -1 || mapHeight == -1) {
//        return false;
//    }
//    else { // allocate our map data
//        levelData = new int*[mapHeight];
//        for(int i = 0; i < mapHeight; ++i) {
//            levelData[i] = new int[mapWidth];
//        }
//        return true;
//    }
//}

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
    if (type == "Player") {
        player.entityType = ENTITY_PLAYER;
        player.isStatic = false;
        player.position = Vector3(x, y, 0.0f);
        player.velocity = Vector3(0.0f, 0.0f, 0.0f);
        player.acceleration = Vector3(0.0f, -1.0f, 0.0f);
        player.render = true;
        player.sprite = SheetSprite(sheet, 98);
        player.size.x = player.sprite.width;
        player.size.y = player.sprite.height;
        player.modelMatrix.Identity();
        player.modelMatrix.Translate(x, y, 1.0f);
        
        //then move/create modelviewmatrix and setup view matrix
        viewMatrix.Identity();
        viewMatrix.Translate(-player.position.x, -player.position.y, 1.0f);
        player.modelviewMatrix = viewMatrix * player.modelMatrix;
    }
    else {
        coin.entityType = ENTITY_COIN;
        coin.isStatic = false;
        coin.position = Vector3(x, y, 0.0f);
        coin.velocity = Vector3(0.0f, 0.0f, 0.0f);
        coin.acceleration = Vector3(0.0f, -1.0f, 0.0f);
        coin.render = true;
        coin.sprite = SheetSprite(sheet, 86);
        coin.size.x = coin.sprite.width;
        coin.size.y = coin.sprite.height;
        coin.modelMatrix.Identity();
        coin.modelMatrix.Translate(x, y, 1.0f);
        
        //then move/create modelviewmatrix and setup view matrix
        viewMatrix.Identity();
        viewMatrix.Translate(-player.position.x, -player.position.y, 0.0f);
        coin.modelviewMatrix = viewMatrix * coin.modelMatrix;
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
//        if (line == "[header]") {
//            if(!readHeader(gamedata)) {
//                assert(false);
//            }
//        }
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
    cout << player.position.x << endl;
    player.Update(elapsed);
    player.CollidesWith(&coin);
    coin.Update(elapsed);
    viewMatrix.Identity();
    if(player.position.x < 9.8){
        viewMatrix.Translate(-9.8, -player.position.y - 2.0, 0.0f);
    }
    else if(player.position.x > 45.4) {
        viewMatrix.Translate(-45.4, -player.position.y - 2.0, 0.0f);
    }
    else{
        viewMatrix.Translate(-player.position.x, -player.position.y - 2.0, 0.0f);
    }
//    if(player.position.x <= 45.4) {
//        viewMatrix.Translate(-player.position.x, -player.position.y - 2.0, 0.0f);
//    }
//    else{
//        viewMatrix.Translate(-45.4, -player.position.y -2.0, 0.0f);
//    }
}

void Render() {
    mapModelMatrix.Identity();
    mapMVM = viewMatrix * mapModelMatrix;
    program.SetModelviewMatrix(mapMVM);
    drawMap(&program);
    coin.modelviewMatrix = viewMatrix * coin.modelMatrix;
    program.SetModelviewMatrix(coin.modelviewMatrix);
    coin.Render(program);
    player.modelviewMatrix = viewMatrix * player.modelMatrix;
    program.SetModelviewMatrix(player.modelviewMatrix);
    player.Render(program);
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
    coinSound = Mix_LoadWAV(RESOURCE_FOLDER"coin.wav");
    music = Mix_LoadMUS(RESOURCE_FOLDER"music.mp3");
    
    Mix_PlayMusic(music, -1);
    
    solids = {1,2, 3,  17, 32};
    
    sheet = LoadTexture(RESOURCE_FOLDER"arne_sprites.png");

    ShaderProgram p(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
    program = p;
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glViewport(0, 0, 640, 360);
    Matrix projectionMatrix;
    projectionMatrix.SetOrthoProjection(-9.55f, 9.55f, -4.0f, 4.0f, -1.0f, 1.0f);
    
    
    createMap(RESOURCE_FOLDER"mymap.txt");
    
    
    //Initalize Time Variables
    float lastFrameTicks = 0.0f;
    float accumulator = 0.0f;
    
    SDL_Event event;
    bool done = false;
    while (!done) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
                done = true;
            }
            else if(event.type == SDL_KEYUP){
                if(event.key.keysym.scancode == SDL_SCANCODE_LEFT || event.key.keysym.scancode == SDL_SCANCODE_A){
                    player.acceleration.x = 0.0f;
                }
                else if(event.key.keysym.scancode == SDL_SCANCODE_RIGHT || event.key.keysym.scancode == SDL_SCANCODE_D){
                    player.acceleration.x = 0.0f;
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
            Update(FIXED_TIMESTEP);
            elapsed -= FIXED_TIMESTEP;
            
        }
        accumulator = elapsed;
        
        Render();
        
        SDL_GL_SwapWindow(displayWindow);
    }
    
    Mix_FreeChunk(jump);
    Mix_FreeChunk(coinSound);
    //Mix_FreeMusic(music);
    
    SDL_Quit();
    return 0;
}



