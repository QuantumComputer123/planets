#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

using std::vector, std::array, std::string, std::unique_ptr, std::pair;

#define WINDOW_WIDTH 1000.0
#define WINDOW_HEIGHT 750.0

#define rlVertex3fv(v) rlVertex3f((v).x, (v).y, (v).z)

#define SPACE_CENTER Vector3{0.0f, 0.0f, 0.0f}

#define CYAN CLITERAL(Color){0, 255, 255, 255}

template <typename In, typename Out>
constexpr In cast(Out&& src) noexcept {
	return static_cast<In>(std::forward<Out>(src));
}

const static double G = 6.6743e-11;

struct Body {
	string name;
	double mass;
	double rad;
	Vector3 pos = SPACE_CENTER;
	Color col;
	double vel = 0;

	Body(string name_, double rad_, double mass_, Vector3 pos_, Color col_)
		: name(name_), mass(mass_), rad(rad_), pos(pos_), col(col_) {
		std::clog << name << " successfully initialized\n";
	}

	void draw() {
		DrawSphere(pos, cast<float>(rad), col);
	}
};

struct Orbit {
   private:
	double mu;
	double rcpA;
	double L;

   public:
	Body centralBody;
	double a = 5.0;
	double b = 0.9;
	double omega = 0;
	double ecc;

	double phi;
	double v;

	Color col;

	array<pair<Vector3, Vector3>, 360> orbitPoints = {};

	Orbit(Body centralBody_, double a_, double b_, double w, Color col_)
		: centralBody(centralBody_), a(a_), b(b_), omega(w), col(col_) {
		mu = centralBody.mass * G;
		rcpA = 1.0 / a;
		ecc = sqrt(1 - ((b * b) / (a * a)));
		L = a * (1 - (ecc * ecc));
	}

	double orbitalDist(double theta) {
		return L / (1 + (ecc * cos(theta - omega)));
	}

	double orbitalSpeed(double distance) {
		v = sqrt(mu * ((2.0 / distance) - rcpA));
		phi = acos((L / (distance * v)) > 1.0 ? 1.0 : (L / (distance * v)));
		return v;
	}

	void calculateOrbit() {
		col = Fade(col, 0.5);
		for (size_t i = 0; i < 360; ++i) {
			double iRad = cast<double>(i) * cast<double>(DEG2RAD);
			double iRadOffset = iRad + cast<double>(DEG2RAD);

			orbitPoints.at(i).first = {
				cast<float>(orbitalDist(iRadOffset) * cos(iRadOffset)),
				0.0f,
				cast<float>(orbitalDist(iRadOffset) * sin(iRadOffset))};

			orbitPoints.at(i).second = {
				cast<float>(orbitalDist(iRad) * cos(iRad)),
				0.0f,
				cast<float>(orbitalDist(iRad) * sin(iRad))};
		}
	}
};

struct Planet : Body {
	Body* centralBody;

	Orbit orbit = Orbit(*centralBody, 1, 1, 0, WHITE);

	double SOIrad;
	double distance = 1;
	double time = 0;

	Model gpuModel;

	Planet(double a, double b, double w, double rad_, double mass_, Color col_, string name_, Body* centralBody_ [[clang::lifetimebound]])
		: Body(name_, rad_, mass_, SPACE_CENTER, col_), centralBody(centralBody_) {
		SOIrad = a * pow(mass / centralBody->mass, 0.4);
		orbit = Orbit(*centralBody, a, b, w, col);
		Body::pos = {
			centralBody->pos.x - cast<float>(orbit.orbitalDist(cast<double>(PI))),
			centralBody->pos.y,
			centralBody->pos.z};
		orbit.calculateOrbit();
	}

	void drawSOI() {
		DrawSphereWires(pos, cast<float>(SOIrad), 12, 12, Fade(col, 0.2f));
	}

	void updatePos(float dt) {
		time += cast<double>(dt);

		double mu = centralBody->mass * G;
		double n = sqrt(mu / (orbit.a * orbit.a * orbit.a));
		double M = n * time;

		double E = M;
		for (int i = 0; i < 8; ++i) {
			E = E - (E - orbit.ecc * sin(E) - M) / (1.0 - orbit.ecc * cos(E));
		}

		distance = orbit.a * (1.0 - orbit.ecc * cos(E));

		vel = orbit.orbitalSpeed(distance) + centralBody->vel;

		double X = orbit.a * (cos(E) - orbit.ecc);
		double Z = orbit.a * sin(E) * sqrt(1.0 - (orbit.ecc * orbit.ecc));

		double rotatedX = X * cos(orbit.omega) - Z * sin(orbit.omega);
		double rotatedZ = X * sin(orbit.omega) + Z * cos(orbit.omega);

		pos = {cast<float>(rotatedX), 0.0f, cast<float>(rotatedZ)};
		pos += centralBody->pos;
	}

	void drawOrbit() {
		rlBegin(RL_LINES);
		for (size_t i = 0; i < 360; ++i) {
			rlColor4ub(orbit.col.r,
					   orbit.col.g,
					   orbit.col.b,
					   orbit.col.a);

			rlVertex3fv(centralBody->pos + orbit.orbitPoints.at(i).first);

			rlVertex3fv(centralBody->pos + orbit.orbitPoints.at(i).second);
		}
		rlEnd();
	}
};

struct Planets {
	vector<unique_ptr<Planet>> planets;

	void add(Planet planet) {
		planets.push_back(std::make_unique<Planet>(planet));
	}

	Planet* at(size_t n) [[clang::lifetimebound]] { return planets.at(n).get(); }

	Planet* at(string key) [[clang::lifetimebound]] {
		for (auto& planet : planets) {
			if (planet->name == key) {
				return planet.get();
			}
		}
		return planets.at(0).get();
	}

	int index(string key) {
		for (size_t i = 0; (++i)-- < planets.size(); ++i) {
			if (planets.at(i)->name == key) {
				return cast<int>(i);
			}
		}
		return 0;
	}

	template <auto... Funcs, typename... Args>
	void run(Args&&... args) {
		for (auto& planet : planets) {
			(std::invoke(Funcs, *planet, std::forward<Args>(args)...), ...);
		}
	}

	template <auto... Funcs, typename... Args>
	void run(Body* currentBody, Args&&... args) {
		int idx = index(currentBody->name);
		for (size_t i = cast<size_t>(idx); i + 1 < planets.size(); ++i) {
			(std::invoke(Funcs, planets.at(i), std::forward<Args>(args)...), ...);
		}
		for (size_t i = cast<size_t>(idx); i > 0; --i) {
			(std::invoke(Funcs, planets.at(i), std::forward<Args>(args)...), ...);
		}
	}
};

constexpr void updateCamera(Camera3D& camera, Body* target, float& xAngle, float& yAngle, float& rad, float& scroll, float& dt) {
	scroll = GetMouseWheelMove();
	rad -= scroll * 20.0f;
	if (rad < cast<float>(target->rad) * 3.0f) rad = cast<float>(target->rad) * 3.0f;
	if (rad > cast<float>(target->rad) * 1000.0f) rad = cast<float>(target->rad * 1000.0);

	if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
		Vector2 mouseDelta = GetMouseDelta();

		float speed = 4.0f;

		xAngle += mouseDelta.x * speed * dt;

		yAngle += mouseDelta.y * speed * dt;

		if (yAngle > 1.55f) yAngle = 1.55f;
		if (yAngle < -1.55f) yAngle = -1.55f;
	}

	camera.target = target->pos;

	camera.position.x = sinf(xAngle) * cosf(yAngle) * rad;
	camera.position.y = sinf(yAngle) * rad;
	camera.position.z = cosf(xAngle) * cosf(yAngle) * rad;

	camera.position += target->pos;
}

int main() {
	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Planets");

	rlSetClipPlanes(cast<double>(0.001), cast<double>(1e6));
	Camera3D camera = {};
	camera.position = Vector3{0.0f, 10.0f, 10.0f};
	camera.target = SPACE_CENTER;
	camera.up = Vector3{0.0f, 1.0f, 0.0f};
	camera.fovy = 60.0f;
	camera.projection = CAMERA_PERSPECTIVE;

	float cameraAngleX = 0.0f;
	float cameraAngleY = 5.0f;
	float cameraRadius = 15.0f;

	if (cameraAngleY > 1.55f) cameraAngleY = 1.55f;
	if (cameraAngleY < -1.55f) cameraAngleY = -1.55f;

	Body sun = Body({"Sun", 1000, 1e15, SPACE_CENTER, YELLOW});

	Body* camCurrentBody = &sun;
	int camCurrentIdx = 0;

	Planets planets = {};

	planets.add(Planet({15000, 15000, 0.0, 25, 1e10, BROWN, "Mercury", &sun}));

	planets.add(Planet({24000, 24000, 0.0, 50, 1e11, YELLOW, "Venus", &sun}));

	planets.add(Planet({36000, 36000, 0.0, 50, 1e11, BLUE, "Earth", &sun}));
	planets.add(Planet({320, 310, 0.0, 1, 2e8, GRAY, "Moon", planets.at("Earth")}));

	planets.add(Planet({48000, 48000, 0.0 * cast<double>(PI), 30, 1e11, RED, "Mars", &sun}));
	planets.add(Planet({220, 220, 0.0, 8, 1.5e8, LIGHTGRAY, "Phobos", planets.at("Mars")}));
	planets.add(Planet({340, 340, 0.0, 5, 1e8, DARKGRAY, "Deimos", planets.at("Mars")}));

	planets.add(Planet({70000, 70000, 0.0, 450, 4e12, ORANGE, "Jupiter", &sun}));

	planets.add(Planet({88000, 88000, 0.0, 380, 4e11, GOLD, "Saturn", &sun}));

	planets.add(Planet({106000, 106000, 0.0, 280, 2e11, CYAN, "Uranus", &sun}));

	planets.add(Planet({124000, 124000, 0.0, 270, 1.9e11, DARKBLUE, "Neptune", &sun}));

	float dt;
	float scroll = 0;
	int speed = 1;

	while (!WindowShouldClose()) {
		dt = GetFrameTime();

		if (IsKeyPressed(KEY_PERIOD)) {
			speed *= 4;
		} else if (IsKeyPressed(KEY_COMMA)) {
			speed /= 4;
		}

		if (speed < 1) speed = 1;

		if (speed > 16384) speed = 1;

		updateCamera(camera, camCurrentBody, cameraAngleX, cameraAngleY, cameraRadius, scroll, dt);

		if (IsKeyPressed(KEY_RIGHT)) {
			camCurrentIdx++;
			if (camCurrentIdx == cast<int>(planets.planets.size() + 1)) {
				camCurrentBody = &sun;
				camCurrentIdx = 0;
			} else if (camCurrentIdx > 0) {
				camCurrentBody = planets.at(cast<size_t>(camCurrentIdx - 1));
			}
			camera.position = camCurrentBody->pos + Vector3{0.0f, 10.0f, 0.0f};
		} else if (IsKeyPressed(KEY_LEFT)) {
			camCurrentIdx--;
			if (camCurrentIdx <= -1) {
				camCurrentIdx = cast<int>(planets.planets.size());
				camCurrentBody = planets.planets.back().get();
			} else if (camCurrentIdx == 0) {
				camCurrentBody = &sun;
			} else {
				camCurrentBody = planets.at(cast<size_t>(camCurrentIdx - 1));
			}
		}

		camera.target = camCurrentBody->pos;

		BeginDrawing();
		ClearBackground(BLACK);
		BeginMode3D(camera);
		sun.draw();
		planets.run<&Planet::drawOrbit, &Planet::draw, &Planet::drawSOI>();
		planets.run<&Planet::updatePos>(cast<float>(speed) * dt);
		EndMode3D();
		DrawFPS(20, 20);
		DrawText(TextFormat("Velocity: %f", camCurrentBody->vel), 20, 50, 18, GREEN);
		DrawText(TextFormat("SPEED: %d", speed), 20, 80, 18, BLUE);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
