#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // transformations
#include <glm/gtx/vector_angle.hpp> // for rotation angles

#include "Geometry.h"
#include "GLDebug.h"
#include "Log.h"
#include "ShaderProgram.h"
#include "Shader.h"
#include "Texture.h"
#include "Window.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"


struct GameObject {
	GameObject(std::string texturePath, GLenum textureInterpolation, int id) :
		texture(texturePath, textureInterpolation),
		position(0.0f, 0.0f, 0.0f),
		velocity(0.01f, 0.01f, 0.0f), // for pickups
		theta(0),
		scale(1),
		transformationMatrix(1.0f), // identity matrix
        collected(false),
        id(),
        hasMoved(false)
	{}

	CPU_Geometry cgeom;
	GPU_Geometry ggeom;
	Texture texture;

	glm::vec3 position;
	glm::vec3 velocity; // movement
	float theta; // rotation
    float targetTheta; // target rotation (for smooth rotation)
	float scale;
	glm::mat4 transformationMatrix;
    bool collected;
    int id;
    bool hasMoved; // flag to check if ship has moved
};


class MyCallbacks : public CallbackInterface {

public:
    MyCallbacks(ShaderProgram& shader, GameObject& ship, std::vector<GameObject*>& pickups, GLFWwindow* window, int& score, bool& won, float& shipStartX, float& shipStartY) 
        : shader(shader), ship(ship), pickups(pickups), window(window), score(score), won(won), shipStartX(shipStartX), shipStartY(shipStartY) {}

	virtual void keyCallback(int key, int scancode, int action, int mods) override {
		if (key == GLFW_KEY_R && action == GLFW_PRESS) {
			resetGame();
		}
	}

    void cursorPosCallback(double xpos, double ypos) override {
        int width, height;
        glfwGetWindowSize(window, &width, &height);

        // convert mouse position to normalized device coordinates (NDC)
        float xNDC = (2.0f * xpos) / width - 1.0f;
        float yNDC = 1.0f - (2.0f * ypos) / height;

        // project NDC to world space (assuming no camera transformation for simplicity)
        glm::vec3 mousePosWorld(xNDC, yNDC, 0.0f);

        // direction vector from ship to mouse position in world space
        glm::vec3 direction = mousePosWorld - ship.position;

        // angle ship should rotate to face mouse (in radians)
        ship.targetTheta = atan2(direction.y, direction.x);
    }   

    void resetGame() {
        // reset ship
        ship.position = glm::vec3(shipStartX, shipStartY, 0.0f);
        ship.scale = 0.08f;
        ship.hasMoved = false;
        score = 0;
        won = false;

        // reset pickups
        for (auto& pickup : pickups) {
            pickup->collected = false;
        }

        std::cout << "---- Game has been reset ----" << std::endl;
    }

private:
	ShaderProgram& shader;
    GameObject& ship;
    GLFWwindow* window;
    std::vector<GameObject*>& pickups;
    int& score;
    bool& won;
    float& shipStartX;
    float& shipStartY;
};

CPU_Geometry createQuad() {
    CPU_Geometry retGeom;
    retGeom.verts = {
        glm::vec3(-1.f,  1.f, 0.f),
        glm::vec3(-1.f, -1.f, 0.f),
        glm::vec3( 1.f, -1.f, 0.f),
        glm::vec3(-1.f,  1.f, 0.f),
        glm::vec3( 1.f, -1.f, 0.f),
        glm::vec3( 1.f,  1.f, 0.f)
    };
    retGeom.texCoords = {
        glm::vec2(0.f, 1.f),
        glm::vec2(0.f, 0.f),
        glm::vec2(1.f, 0.f),
        glm::vec2(0.f, 1.f),
        glm::vec2(1.f, 0.f),
        glm::vec2(1.f, 1.f)
    };
    return retGeom;
}

bool isCloseTo(const GameObject& ship, const GameObject& pickup) {
    float distance = glm::length(ship.position - pickup.position);
    return distance < 0.1f; // threshold for collecting the pickup
}

void updateGameObject(GameObject& obj, float deltaTime, GLFWwindow* window) {
    // move player forward/backward using keyboard input
    const float rotationSpeed = 3.0f; // rotation speed (higher = faster rotation)
    const float moveSpeed = 2.0f;     // movement speed

	// smooth rotation toward target angle
    obj.theta = glm::mix(obj.theta, obj.targetTheta, deltaTime * rotationSpeed);

    // move player forward/backward using keyboard input (w&s)
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        obj.position += glm::vec3(cos(obj.theta), sin(obj.theta), 0.0f) * deltaTime * moveSpeed;
        obj.hasMoved = true; // moving
    }

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        obj.position -= glm::vec3(cos(obj.theta), sin(obj.theta), 0.0f) * deltaTime * moveSpeed;
        obj.hasMoved = true; // moving
    }
}

void updatePickup(GameObject& pickup, float screenBoundary) {
    if (!pickup.collected) {
        // move pick-up and bounce off screen edges
        pickup.position += pickup.velocity * 0.2f;

        if (pickup.position.x > screenBoundary || pickup.position.x < -screenBoundary) {
            pickup.velocity.x = -pickup.velocity.x;
        }

        if (pickup.position.y > screenBoundary || pickup.position.y < -screenBoundary) {
            pickup.velocity.y = -pickup.velocity.y;
        }
    }
}

int main() {
	Log::debug("Starting main");

	// WINDOW
	glfwInit();
	Window window(800, 800, "CPSC 453");

	GLDebug::enable();

    glfwSetInputMode(window.getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	// SHADERS
	ShaderProgram shader("shaders/test.vert", "shaders/test.frag");

	// CALLBACKS
	GameObject ship("textures/ship.png", GL_NEAREST, 0);

	GameObject pickup1("textures/diamond.png", GL_NEAREST, 1);
    GameObject pickup2("textures/diamond.png", GL_NEAREST, 2);
    GameObject pickup3("textures/diamond.png", GL_NEAREST, 3);
    GameObject pickup4("textures/diamond.png", GL_NEAREST, 4);

    std::vector<GameObject*> pickups = { &pickup1, &pickup2, &pickup3, &pickup4 };

    for (auto& pickup : pickups) {
        pickup->cgeom = createQuad();
        pickup->ggeom.setVerts(pickup->cgeom.verts);
        pickup->ggeom.setTexCoords(pickup->cgeom.texCoords);
        pickup->scale = 0.05f;
        pickup->collected = false;
    }

    pickup1.id = 1;
    pickup2.id = 2;
    pickup3.id = 3;
    pickup4.id = 4;

	ship.cgeom = createQuad();
    ship.ggeom.setVerts(ship.cgeom.verts);
    ship.ggeom.setTexCoords(ship.cgeom.texCoords);
	ship.scale = 0.08f;

	// pickup initial positions and velocities/directions
    pickup1.position = glm::vec3(0.2f, 0.5f, 0.0f);
    pickup1.velocity = glm::vec3(0.01f, 0.01f, 0.0f);

    pickup2.position = glm::vec3(-0.5f, 0.0f, 0.0f);
    pickup2.velocity = glm::vec3(-0.01f, -0.01f, 0.0f);

    pickup3.position = glm::vec3(0.5f, -0.2f, 0.0f);
    pickup3.velocity = glm::vec3(0.01f, -0.01f, 0.0f);

    pickup4.position = glm::vec3(0.0f, 0.1f, 0.0f);
    pickup4.velocity = glm::vec3(-0.01f, 0.01f, 0.0f); 

    float lastTime = glfwGetTime();
    int score = 0;
    bool won = false;
    float shipStartX = ship.position.x;
    float shipStartY = ship.position.y;

    window.setCallbacks(std::make_shared<MyCallbacks>(shader, ship, pickups, window.getWindow(), score, won, shipStartX, shipStartY));

	// RENDER LOOP
	while (!window.shouldClose()) {
		float currentTime = glfwGetTime();
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        glfwPollEvents();

		// RENDERING
		glEnable(GL_FRAMEBUFFER_SRGB);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		shader.use();

		// update ship (player) and pick-ups
        updateGameObject(ship, deltaTime, window.getWindow());

        // render ship
        if (!won) {
            // apply transformations via matrix
            glm::mat4 model = glm::translate(glm::mat4(1.0f), ship.position);
            float rotationCorrection = glm::radians(90.0f);
            model = glm::rotate(model, ship.theta - rotationCorrection, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, glm::vec3(ship.scale, ship.scale, 1.0f));
            shader.setUniformMat4("transformationMatrix", model);

            ship.ggeom.bind();
            ship.texture.bind();
            glDrawArrays(GL_TRIANGLES, 0, 6);
            ship.texture.unbind();
        }

        // render pickups
        for (auto& pickup : pickups) {
            updatePickup(*pickup, 0.9f);

            if (!pickup->collected) {
                glm::mat4 pickupModel = glm::translate(glm::mat4(1.0f), pickup->position);
                pickupModel = glm::scale(pickupModel, glm::vec3(pickup->scale, pickup->scale, 1.0f));
                shader.setUniformMat4("transformationMatrix", pickupModel);
                    
                pickup->texture.bind();
                pickup->ggeom.bind();

                glDrawArrays(GL_TRIANGLES, 0, pickup->cgeom.verts.size());
                pickup->texture.unbind();
            } 
        }

        if (ship.hasMoved) {
            // check for collision with pickups only if the ship has moved
            for (auto& pickup : pickups) {
                if (isCloseTo(ship, *pickup) && !pickup->collected) {
                    pickup->collected = true;
                    score++;
                    ship.scale += 0.02f; // ship grows on collecting a pickup
                    std::cout << "Collected pickup" << pickup->id << "!" << std::endl;
                }
            }
        }

        // count collected pickups and check for win condition
        int collectedCount = 0;

        for (auto& pickup : pickups) {
            if (pickup->collected) {
                collectedCount++;
            }
        }

        if (collectedCount == pickups.size()) {
            won = true;
        }

        glDisable(GL_FRAMEBUFFER_SRGB); // disable sRGB for things like imgui

		// starting the new ImGui frame -- display score
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

        ImGui::Begin("Score");
        if (!won) {
            ImGui::Text("Score: %d", score);
        } else {
            ImGui::Text("Congratulations! You won!");
        }
        ImGui::Text("Press R to restart.");
        ImGui::End();

		ImGui::Render();	// render the ImGui window
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // some middleware thing

		window.swapBuffers();
		glfwPollEvents();
	}

	// ImGui cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwTerminate();
	return 0;
}
