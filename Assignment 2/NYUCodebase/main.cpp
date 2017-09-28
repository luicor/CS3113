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

#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(image);
    return retTexture;
}

SDL_Window* displayWindow;

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif
    
    glViewport(0, 0, 640, 360);
    
    ShaderProgram program(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
    
    Matrix projectionMatrix;
    projectionMatrix.SetOrthoProjection(-3.55f, 3.55f, -2.0f, 2.0f, -1.0f, 1.0f);
    //RIGHT PADDLE
    Matrix modelviewMatrix;
    modelviewMatrix.Translate(-3.30, 0.0f, 0.0f);
    //LEFT PADDLE
    Matrix modelviewMatrix2;
    modelviewMatrix2.Translate(3.30, 0.0f, 0.0f);
    
    //TOP WALL
    Matrix modelviewMatrix3;
    modelviewMatrix3.Translate(0.0f, 2.0f, 0.0f);
    //BOTTOM WALL
    Matrix modelviewMatrix4;
    modelviewMatrix4.Translate(0.0f, -2.0f, 0.0f);
    
    //BALL
    Matrix modelviewMatrix5;
    modelviewMatrix5.Translate(0.0f, 1.0f, 0.0f);
    
    
    
    float lastFrameTicks = 0.0f;
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    float y_position = 0.0f;
    float x_position = 0.0f;
    //Keep track of y position of left paddle
    float leftPaddleYPos = modelviewMatrix.ml[13];
    //Keep track of y position of right paddle
    float rightPaddleYPos = modelviewMatrix2.ml[13];
    //Keep track of x and y position of ball
    float ballYPos = modelviewMatrix5.ml[13];
    float ballXPos = modelviewMatrix5.ml[12];
    
    //BALL AND HEIGHT DIMENSIONS SET FOR USE LATER ON WITH COLLISIONS
    float paddleHeight = 1.0f;
    float paddleWidth = 0.16f;
    float ballHeight = 0.16f;
    float ballWidth = 0.16f;
    
    float ballTop;
    float ballBottom;
    float ballRight;
    float ballLeft;
    
    float leftTop;
    float leftBottom;
    float leftRight;
    float leftLeft;
    
    float rightTop;
    float rightBottom;
    float rightRight;
    float rightLeft;
    
    //To know if ball should be moving
    bool move = false;
    //0 means right 1 means left
    int x_direction = 0;
    //0 means up 1 means down
    int y_direction = 0;
    
	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
            else if(event.type == SDL_KEYDOWN) {//To start ball movement
                if(event.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                    move = true;
                }
            }
		}
        
        float ticks = (float)SDL_GetTicks()/1000.0f;
        float elapsed = ticks - lastFrameTicks;
        lastFrameTicks = ticks;
        
        y_position = elapsed * 2.0f;//how much to move things in y direction
        x_position = elapsed * 2.0f;//how much to move things in x direction
        
        
		glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(program.programID);
        
        program.SetProjectionMatrix(projectionMatrix);
        
        
        //MAKING THE TOP AND BOTTOM WALL BOUNDARIES
        program.SetModelviewMatrix(modelviewMatrix3);
        
        float vertices3[] = {-3.55, -0.1, 3.55, -0.1, 3.55, 0.0, -3.55, -0.1, 3.55, 0.0, -3.55, 0.0};
        glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices3);
        glEnableVertexAttribArray(program.positionAttribute);


        glDrawArrays(GL_TRIANGLES, 0, 6);

        glDisableVertexAttribArray(program.positionAttribute);
        
        program.SetModelviewMatrix(modelviewMatrix4);
        
        float vertices4[] = {-3.55, 0.0, 3.55, 0.1, 3.55, 0.0, -3.55, 0.0, 3.55, 0.1, -3.55, 0.1};
        glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices4);
        glEnableVertexAttribArray(program.positionAttribute);
        
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        glDisableVertexAttribArray(program.positionAttribute);
        
        
        //MAKING THE RIGHT AND LEFT PADDLES
        program.SetModelviewMatrix(modelviewMatrix);
        
        float vertices[] = {-0.08, -0.5, 0.08, -0.5, 0.08, 0.5, -0.08, -0.5, 0.08, 0.5, -0.08, 0.5};
        glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
        glEnableVertexAttribArray(program.positionAttribute);
        
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        glDisableVertexAttribArray(program.positionAttribute);
        
        program.SetModelviewMatrix(modelviewMatrix2);
        
        float vertices2[] = {-0.08, -0.5, 0.08, -0.5, 0.08, 0.5, -0.08, -0.5, 0.08, 0.5, -0.08, 0.5};
        glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices2);
        glEnableVertexAttribArray(program.positionAttribute);
        
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        glDisableVertexAttribArray(program.positionAttribute);
        
        //BALL CODE
        program.SetModelviewMatrix(modelviewMatrix5);
        float vertices5[] = {-0.08, -0.08, 0.08, -0.08, 0.08, 0.08, -0.08, -0.08, 0.08, 0.08, -0.08, 0.08};
        glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices5);
        glEnableVertexAttribArray(program.positionAttribute);
        
        
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        glDisableVertexAttribArray(program.positionAttribute);
        
        //KEEPS TRACK OF ALL BALL AND PADDLE SIDES FOR COLLISION DETECTION
        ballTop = modelviewMatrix5.ml[13] + (ballHeight/2);
        ballBottom = modelviewMatrix5.ml[13] - (ballHeight/2);
        ballRight = modelviewMatrix5.ml[12] + (ballWidth/2);
        ballLeft = modelviewMatrix5.ml[12] - (ballWidth/2);
        
        rightTop = modelviewMatrix2.ml[13] + (paddleHeight/2);
        rightBottom = modelviewMatrix2.ml[13] - (paddleHeight/2);
        rightRight = modelviewMatrix2.ml[12] + (paddleWidth/2);
        rightLeft = modelviewMatrix2.ml[12] - (paddleWidth/2);
        
        leftTop = modelviewMatrix.ml[13] + (paddleHeight/2);
        leftBottom = modelviewMatrix.ml[13] - (paddleHeight/2);
        leftRight = modelviewMatrix.ml[12] + (paddleWidth/2);
        leftLeft = modelviewMatrix.ml[12] - (paddleWidth/2);
        
        //RESET AFTER A WIN AND BALL GOES IN DIRECTION OF LOSER
        if(ballLeft < -3.55) {
            move = false;
            modelviewMatrix.Identity();
            modelviewMatrix2.Identity();
            modelviewMatrix5.Identity();
            modelviewMatrix.Translate(-3.30f, 0.0f, 0.0f);
            modelviewMatrix2.Translate(3.30f, 0.0f, 0.0f);
            modelviewMatrix5.Translate(0.0f, 1.0f, 0.0f);
            ballXPos = modelviewMatrix5.ml[12];
            ballYPos = modelviewMatrix5.ml[13];
            rightPaddleYPos = modelviewMatrix2.ml[13];
            leftPaddleYPos = modelviewMatrix.ml[13];
            x_direction = 1;
            y_direction = 0;
        }
        else if(ballRight > 3.55) {
            move = false;
            modelviewMatrix.Identity();
            modelviewMatrix2.Identity();
            modelviewMatrix5.Identity();
            modelviewMatrix.Translate(-3.30f, 0.0f, 0.0f);
            modelviewMatrix2.Translate(3.30f, 0.0f, 0.0f);
            modelviewMatrix5.Translate(0.0f, 1.0f, 0.0f);
            ballXPos = modelviewMatrix5.ml[12];
            ballYPos = modelviewMatrix5.ml[13];
            rightPaddleYPos = modelviewMatrix2.ml[13];
            leftPaddleYPos = modelviewMatrix.ml[13];
            x_direction = 0;
            y_direction = 0;
        }
        
        if(move == true) {//HANDLES ALL BALL MOVEMENT AND COLLISION HANDLING
            if(x_direction == 0) {
                if(ballBottom < rightTop && ballTop > rightBottom && ballLeft < rightRight && ballRight > rightLeft) {
                    if(y_direction == 0) {
                        modelviewMatrix5.Identity();
                        modelviewMatrix5.Translate(ballXPos - x_position, ballYPos + y_position, 0.0f);
                        ballXPos -= x_position;
                        ballYPos += y_position;
                        x_direction = 1;
                    }
                    else{
                        modelviewMatrix5.Identity();
                        modelviewMatrix5.Translate(ballXPos - x_position, ballYPos - y_position, 0.0f);
                        ballXPos -= x_position;
                        ballYPos -= y_position;
                        x_direction = 1;
                    }
                    
                }
                else if(ballTop > 1.9f) {//COLLISION WITH TOP WALL
                    modelviewMatrix5.Identity();
                    modelviewMatrix5.Translate(ballXPos + x_position, ballYPos - y_position, 0.0f);
                    ballXPos += x_position;
                    ballYPos -= y_position;
                    y_direction = 1;
                }
                else if(ballBottom < -1.9f) {//COLLISION WITH BOTTOM WALL
                    modelviewMatrix5.Identity();
                    modelviewMatrix5.Translate(ballXPos + x_position, ballYPos + y_position, 0.0f);
                    ballXPos += x_position;
                    ballYPos += y_position;
                    y_direction = 0;
                }
                else{
                    if(y_direction == 0) {
                        modelviewMatrix5.Identity();
                        modelviewMatrix5.Translate(ballXPos + x_position, ballYPos + y_position, 0.0f);
                        ballXPos += x_position;
                        ballYPos += y_position;
                    }
                    else{
                        modelviewMatrix5.Identity();
                        modelviewMatrix5.Translate(ballXPos + x_position, ballYPos - y_position, 0.0f);
                        ballXPos += x_position;
                        ballYPos -= y_position;
                    }
                }
            }
            else{
                if(ballBottom < leftTop && ballTop > leftBottom && ballLeft < leftRight && ballRight > leftLeft) {
                    if(y_direction == 0) {
                        modelviewMatrix5.Identity();
                        modelviewMatrix5.Translate(ballXPos + x_position, ballYPos + y_position, 0.0f);
                        ballXPos += x_position;
                        ballYPos += y_position;
                        x_direction = 0;
                    }
                    else{
                        modelviewMatrix5.Identity();
                        modelviewMatrix5.Translate(ballXPos + x_position, ballYPos - y_position, 0.0f);
                        ballXPos += x_position;
                        ballYPos -= y_position;
                        x_direction = 0;
                    }
                    
                }
                else if(ballTop > 1.9f) {//COLLISION WITH TOP WALL
                    modelviewMatrix5.Identity();
                    modelviewMatrix5.Translate(ballXPos - x_position, ballYPos - y_position, 0.0f);
                    ballXPos -= x_position;
                    ballYPos -= y_position;
                    y_direction = 1;
                }
                else if(ballBottom < -1.9f) {//COLLISION WITH BOTTOM WALL
                    modelviewMatrix5.Identity();
                    modelviewMatrix5.Translate(ballXPos - x_position, ballYPos + y_position, 0.0f);
                    ballXPos -= x_position;
                    ballYPos += y_position;
                    y_direction = 0;
                }
                else{
                    if(y_direction == 0) {
                        modelviewMatrix5.Identity();
                        modelviewMatrix5.Translate(ballXPos - x_position, ballYPos + y_position, 0.0f);
                        ballXPos -= x_position;
                        ballYPos += y_position;
                    }
                    else{
                        modelviewMatrix5.Identity();
                        modelviewMatrix5.Translate(ballXPos - x_position, ballYPos - y_position, 0.0f);
                        ballXPos -= x_position;
                        ballYPos -= y_position;
                    }
                }
            }
        }
        
        //LOOKING TO SEE IF RIGHT OR LEFT PADDLE SHOULD MOVE
        if(keys[SDL_SCANCODE_W]) {
            if(leftTop < 1.9f) {//checks to see collision with top
                modelviewMatrix.Identity();
                modelviewMatrix.Translate(-3.30f, leftPaddleYPos + y_position, 0.0f);
                leftPaddleYPos += y_position;
            }
        }
        else if(keys[SDL_SCANCODE_S]) {
            if(leftBottom > -1.9f) {//checks to see collision with bottom
                modelviewMatrix.Identity();
                modelviewMatrix.Translate(-3.30f, leftPaddleYPos - y_position, 0.0f);
                leftPaddleYPos -= y_position;
            }
        }
        
        if(keys[SDL_SCANCODE_UP]) {
            if(rightTop < 1.9f) {
                modelviewMatrix2.Identity();
                modelviewMatrix2.Translate(3.30f, rightPaddleYPos + y_position, 0.0f);
                rightPaddleYPos += y_position;
            }
        }
        else if(keys[SDL_SCANCODE_DOWN]) {
            if(rightBottom > -1.9f) {
                modelviewMatrix2.Identity();
                modelviewMatrix2.Translate(3.30f, rightPaddleYPos - y_position, 0.0f);
                rightPaddleYPos -= y_position;
            }
        }
        
		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}
