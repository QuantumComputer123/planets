#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

using std::vector, std::string, std::unique_ptr;

#define WINDOW_WIDTH 1000.0
#define WINDOW_HEIGHT 750.0

#define WINDOW_CENTER Vector2{WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2}

template <typename In, typename Out>
constexpr In cast(Out&& src) noexcept {
	return static_cast<In>(std::forward<Out>(src));
}

const static double G = 6.6743e-11;

struct Body {
	string name;
	double mass;
	double rad;
	Vector2 pos = WINDOW_CENTER;
	Color col;
	double vel = 0;

	Body(string name_, double rad_, double mass_, Vector2 pos_, Color col_)
		: name(name_), mass(mass_), rad(rad_), pos(pos_), col(col_) {
		std::clog << name << " successfully initialized\n";
	}

	void draw() {
		DrawCircleV(pos, cast<float>(rad), col);
	}
};

struct Orbit {
   private:
	double mu;
	double rcpA;
	double L;

   public:
	Body centralBody;
	double a = 5;
	double b = 0.9;
	double omega = 0;
	double ecc;

	double phi;
	double v;

	Orbit(Body centralBody_, double a_, double b_, double w)
		: centralBody(centralBody_), a(a_), b(b_), omega(w) {
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
};

struct Planet : Body {
	Body* centralBody;

	Orbit orbit = Orbit(*centralBody, 1, 1, 0);

	double SOIrad;
	double distance = 1;
	double time = 0;

	Planet(double a, double b, double w, double rad_, double mass_, Color col_, string name_, Body* centralBody_ [[clang::lifetimebound]])
		: Body(name_, rad_, mass_, WINDOW_CENTER, col_), centralBody(centralBody_) {
		SOIrad = a * pow(mass / centralBody->mass, 0.4);
		orbit = Orbit(*centralBody, a, b, w);
		Body::pos = {centralBody->pos.x - cast<float>(orbit.orbitalDist(cast<double>(PI))), centralBody->pos.y};
	}

	void draw() {
		DrawCircleV(pos, float(rad), col);
	}

	void drawOrbit() {
		rlBegin(RL_LINES);
		for (int i = 0; i < 360; i += 1) {
			double iRad = cast<double>(i) * cast<double>(DEG2RAD);
			double iRadOffset = iRad + cast<double>(DEG2RAD);

			rlColor4ub(cast<unsigned char>(0.5 * cast<int>(col.r)),
					   cast<unsigned char>(0.5 * cast<int>(col.g)),
					   cast<unsigned char>(0.5 * cast<int>(col.b)),
					   cast<unsigned char>(0.5 * cast<int>(col.a)));

			rlVertex2f(
				cast<float>(cast<double>(centralBody->pos.x) + cast<double>(orbit.orbitalDist(iRadOffset) * cos(iRadOffset))),
				cast<float>(cast<double>(centralBody->pos.y) + cast<double>(orbit.orbitalDist(iRadOffset) * sin(iRadOffset))));

			rlVertex2f(
				cast<float>(cast<double>(centralBody->pos.x) + cast<double>(orbit.orbitalDist(iRad) * cos(iRad))),
				cast<float>(cast<double>(centralBody->pos.y) + cast<double>(orbit.orbitalDist(iRad) * sin(iRad))));
		}
		rlEnd();
	}

	void drawSOI() {
		DrawCircleV(pos, cast<float>(SOIrad), Fade(col, 0.2f));
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

		double X = orbit.a * (cos(E) - orbit.ecc);
		double Y = orbit.a * sin(E) * sqrt(1.0 - (orbit.ecc * orbit.ecc));

		double rotatedX = X * cos(orbit.omega) - Y * sin(orbit.omega);
		double rotatedY = X * sin(orbit.omega) + Y * cos(orbit.omega);

		pos = {cast<float>(rotatedX), cast<float>(rotatedY)};
		pos += centralBody->pos;

		distance = cast<double>(Vector2Distance(centralBody->pos, pos));
		vel = orbit.orbitalSpeed(distance);
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

	template <auto... Funcs, typename... Args>
	void run(Args&&... args) {
		for (auto& planet : planets) {
			(std::invoke(Funcs, *planet, std::forward<Args>(args)...), ...);
		}
	}
};

int main() {
	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Planets");

	Camera2D camera = {};
	camera.target = WINDOW_CENTER;
	camera.offset = WINDOW_CENTER;
	camera.rotation = 0.0f;
	camera.zoom = 1.0f;

	Body sun = Body({"Sun", 25, 1e12, WINDOW_CENTER, YELLOW});

	Body* camCurrentBody = &sun;
	int camCurrentIdx = 0;

	Planets planets = {};

	planets.add(Planet({360, 360, 0.875 * cast<double>(PI), 0.5, 1e7, BLUE, "Earth", &sun}));
	planets.add(Planet({2.3, 2.3, 0.0, 0.1, 2e5, GRAY, "Moon", planets.at("Earth")}));

	float dt;
	int speed = 1;

	while (!WindowShouldClose()) {
		dt = cast<float>(speed) * GetFrameTime();

		if (IsKeyPressed(KEY_PERIOD)) {
			speed *= 2;
		} else if (IsKeyPressed(KEY_COMMA)) {
			speed /= 2;
		}

		if (speed < 1) speed = 1;

		if (speed > 1024) speed = 1;

		camera.zoom = expf(logf(camera.zoom) + (cast<float>(GetMouseWheelMove()) * 0.1f));

		if (camera.zoom > 30.0f / cast<float>(camCurrentBody->rad))
			camera.zoom = 30.0f / cast<float>(camCurrentBody->rad);
		else if (camera.zoom < 0.1f)
			camera.zoom = 0.1f;

		if (IsKeyPressed(KEY_RIGHT)) {
			camCurrentIdx++;
			if (camCurrentIdx == cast<int>(planets.planets.size() + 1)) {
				camCurrentBody = &sun;
				camCurrentIdx = 0;
			} else if (camCurrentIdx > 0) {
				camCurrentBody = planets.at(cast<size_t>(camCurrentIdx - 1));
			}
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
		BeginMode2D(camera);
		sun.draw();
		planets.run<&Planet::drawOrbit, &Planet::drawSOI, &Planet::draw>();
		planets.run<&Planet::updatePos>(dt);
		EndMode2D();
		DrawFPS(20, 20);
		DrawText(TextFormat("Velocity: %f", camCurrentBody->vel), 20, 50, 18, GREEN);
		DrawText(TextFormat("SPEED: %d", speed), 20, 80, 18, BLUE);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
