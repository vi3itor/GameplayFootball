// written by bastiaan konings schuiling 2008 - 2015
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)
//
// main.cpp — program entry point and global state.
// Owns all top-level singletons (systems, scenes, tasks, controllers) and wires
// them together before handing control to the scheduler.  On exit it tears them
// down in reverse order.

#ifdef WIN32
#include <windows.h>
#endif

#include "main.hpp"

#include "base/utils.hpp"
#include "base/math/bluntmath.hpp"

#include "scene/scene2d/scene2d.hpp"
#include "scene/scene3d/scene3d.hpp"

#include "managers/resourcemanagerpool.hpp"
#include "utils/objectloader.hpp"
#include "scene/objectfactory.hpp"

#include "systems/audio/audio_system.hpp"

#include "framework/scheduler.hpp"

#include "managers/systemmanager.hpp"
#include "managers/scenemanager.hpp"

#include "base/log.hpp"

#include "types/thread.hpp"
#include "utils/threadhud.hpp"

#include "utils/orbitcamera.hpp"

#include "SDL2/SDL_ttf.h"

#if defined(WIN32) && defined(__MINGW32__)
#undef main
#endif

using namespace blunted;

GraphicsSystem *graphicsSystem;
AudioSystem *audioSystem;

boost::shared_ptr<Scene2D> scene2D;  // 2D overlay scene (HUD, menus, debug images)
boost::shared_ptr<Scene3D> scene3D;  // 3D world scene (pitch, players, ball)

// Two scheduler sequences run concurrently: game logic and rendering.
boost::shared_ptr<TaskSequence> graphicsSequence;
boost::shared_ptr<TaskSequence> gameSequence;

boost::shared_ptr<GameTask> gameTask;
boost::shared_ptr<MenuTask> menuTask;

// Coloured marker objects placed anywhere in the 3D scene to visualise AI/physics points.
boost::intrusive_ptr<Geometry> greenPilon;
boost::intrusive_ptr<Geometry> bluePilon;
boost::intrusive_ptr<Geometry> yellowPilon;
boost::intrusive_ptr<Geometry> redPilon;

// Additional debug shapes for visualising radii or areas of interest.
boost::intrusive_ptr<Geometry> smallDebugCircle1;
boost::intrusive_ptr<Geometry> smallDebugCircle2;
boost::intrusive_ptr<Geometry> largeDebugCircle;

void SetGreenDebugPilon(const Vector3 &pos) { greenPilon->SetPosition(pos, false); }
void SetBlueDebugPilon(const Vector3 &pos) { bluePilon->SetPosition(pos, false); }
void SetYellowDebugPilon(const Vector3 &pos) { yellowPilon->SetPosition(pos, false); }
void SetRedDebugPilon(const Vector3 &pos) { redPilon->SetPosition(pos, false); }

void SetSmallDebugCircle1(const Vector3 &pos) { smallDebugCircle1->SetPosition(pos, false); }
void SetSmallDebugCircle2(const Vector3 &pos) { smallDebugCircle2->SetPosition(pos, false); }
void SetLargeDebugCircle(const Vector3 &pos) { largeDebugCircle->SetPosition(pos, false); }

boost::intrusive_ptr<Geometry> GetGreenDebugPilon() { return greenPilon; }
boost::intrusive_ptr<Geometry> GetBlueDebugPilon() { return bluePilon; }
boost::intrusive_ptr<Geometry> GetYellowDebugPilon() { return yellowPilon; }
boost::intrusive_ptr<Geometry> GetRedDebugPilon() { return redPilon; }

boost::intrusive_ptr<Geometry> GetSmallDebugCircle1() { return smallDebugCircle1; }
boost::intrusive_ptr<Geometry> GetSmallDebugCircle2() { return smallDebugCircle2; }
boost::intrusive_ptr<Geometry> GetLargeDebugCircle() { return largeDebugCircle; }

Database *db;       // SQLite player/team database
Properties *config; // key-value config loaded from football.config (or argv[1])

// Full-screen 2D surfaces used for runtime debug rendering (disabled in release builds).
boost::intrusive_ptr<Image2D> debugImage;   // small thumbnail in the corner (superDebug mode)
boost::intrusive_ptr<Image2D> debugOverlay; // full-screen overlay (AI debug mode)

std::vector<IHIDevice*> controllers; // index 0 = keyboard; 1+ = gamepads

bool superDebug = false;
e_DebugMode debugMode = e_DebugMode_Off;

std::string activeSaveDirectory;

std::string configFile = "football.config"; // overridden by argv[1] at startup
std::string GetConfigFilename() {
  return configFile;
}

boost::shared_ptr<Scene2D> GetScene2D() {
  return scene2D;
}

boost::shared_ptr<Scene3D> GetScene3D() {
  return scene3D;
}

GraphicsSystem *GetGraphicsSystem() {
  return graphicsSystem;
}

boost::shared_ptr<GameTask> GetGameTask() {
  return gameTask;
}

boost::shared_ptr<MenuTask> GetMenuTask() {
  return menuTask;
}

Database *GetDB() {
  return db;
}

bool IsReleaseVersion() {
  if (GetConfiguration()->GetBool("debug", false)) return false; else return true;
}

bool Verbose() {
  return !IsReleaseVersion();
}

bool UpdateNonImportableDB() {
  if (IsReleaseVersion()) return false;
  else return true;
}

Properties *GetConfiguration() {
  return config;
}

std::string GetActiveSaveDirectory() {
  return activeSaveDirectory;
}

void SetActiveSaveDirectory(const std::string &dir) {
  activeSaveDirectory = dir;
}

bool SuperDebug() {
  return superDebug;
}

e_DebugMode GetDebugMode() {
  return debugMode;
}

boost::intrusive_ptr<Image2D> GetDebugImage() {
  return debugImage;
}

boost::intrusive_ptr<Image2D> GetDebugOverlay() {
  return debugOverlay;
}

void GetDebugOverlayCoord(Match *match, const Vector3 &worldPos, int &x, int &y) {
  Vector3 proj = GetProjectedCoord(worldPos, match->GetCamera());
  int dud1, dud2;
  GetMenuTask()->GetWindowManager()->GetCoordinates(proj.coords[0], proj.coords[1], 1, 1, x, y, dud1, dud2);

  int contextW, contextH, bpp;
  GetScene2D()->GetContextSize(contextW, contextH, bpp);
  x = clamp(x, 0, contextW - 1);
  y = clamp(y, 0, contextH - 1);
}

// Returns how many milliseconds remain until the next predicted frame swap.
// Used by game logic to decide how much work it can still do this frame.
int PredictFrameTimeToGo_ms(int frameCount) {
  int averageFrameTime_ms = GetGraphicsSystem()->GetAverageFrameTime_ms(frameCount);
  int timeSinceLastSwap_ms = GetGraphicsSystem()->GetTimeSinceLastSwap_ms();
  int timeToNextSwapPrediction_ms = averageFrameTime_ms - timeSinceLastSwap_ms;
  timeToNextSwapPrediction_ms = clamp(timeToNextSwapPrediction_ms, 0, 1000);
  return timeToNextSwapPrediction_ms;
}

void InitDebugImage() {
  SDL_Surface *sdlSurface = CreateSDLSurface(200, 150);

  boost::intrusive_ptr < Resource <Surface> > resource = ResourceManagerPool::GetInstance().GetManager<Surface>(e_ResourceType_Surface)->Fetch("debugimage", false, true);
  Surface *surface = resource->GetResource();

  surface->SetData(sdlSurface);

  debugImage = boost::static_pointer_cast<Image2D>(ObjectFactory::GetInstance().CreateObject("debugimage", e_ObjectType_Image2D));
  scene2D->CreateSystemObjects(debugImage);
  debugImage->SetImage(resource);

  int contextW, contextH, bpp; // context
  scene2D->GetContextSize(contextW, contextH, bpp);
  debugImage->SetPosition(contextW - 210, contextH - 160);

  scene2D->AddObject(debugImage);

  debugImage->DrawRectangle(0, 0, 200, 150, Vector3(40, 20, 20), 100);
  debugImage->OnChange();
}

void InitDebugOverlay() {
  int contextW, contextH, bpp; // context
  scene2D->GetContextSize(contextW, contextH, bpp);

  SDL_Surface *sdlSurface = CreateSDLSurface(contextW, contextH);

  boost::intrusive_ptr < Resource <Surface> > resource = ResourceManagerPool::GetInstance().GetManager<Surface>(e_ResourceType_Surface)->Fetch("debugoverlay", false, true);
  Surface *surface = resource->GetResource();

  surface->SetData(sdlSurface);

  debugOverlay = boost::static_pointer_cast<Image2D>(ObjectFactory::GetInstance().CreateObject("debugoverlay", e_ObjectType_Image2D));
  scene2D->CreateSystemObjects(debugOverlay);
  debugOverlay->SetImage(resource);

  debugOverlay->SetPosition(0, 0);

  scene2D->AddObject(debugOverlay);

  debugOverlay->DrawRectangle(0, 0, contextW, contextH, Vector3(0, 0, 0), 0);
  debugOverlay->OnChange();
}

const std::vector<IHIDevice*> &GetControllers() {
  return controllers;
}

// Background thread that drives the ThreadHud overlay (per-thread timing bars).
// Instantiated only in debug builds when the hud flag is enabled (see main()).
class ThreadHudThread : public Thread {
  public:
    ThreadHudThread() {
      hud = new ThreadHud(GetScene2D());
    }
    virtual ~ThreadHudThread() {
      delete hud;
    }

    virtual void operator()() {
      bool quit = false;
      while (!quit) {
        SetState(e_ThreadState_Busy);

        bool isMessage = false;
        boost::intrusive_ptr<Command> message = boost::intrusive_ptr<Command>();
        message = messageQueue.GetMessage(isMessage);
        if (isMessage) {
          if (!message->Handle(this)) quit = true;
          message.reset();
        }

        hud->Execute();

        SetState(e_ThreadState_Idle);
        boost::this_thread::yield();
      }
    }

  protected:
    ThreadHud *hud;
};


int main(int argc, const char** argv) {

  // --- Config ------------------------------------------------------------------
  config = new Properties();
  if (argc > 1) configFile = argv[1];
  config->LoadFile(configFile.c_str());

  Initialize(*config); // engine-level init (logging, SDL, etc.)

  srand(time(NULL));
  rand(); // discard the first value — MinGW's RNG produces a biased first result
  randomseed();     // seed boost::random
  fastrandomseed(); // seed the lightweight fast-random helper

  // Fixed physics timestep; graphics may render at a different (uncapped) rate.
  int timeStep_ms = config->GetInt("physics_frametime_ms", 10);


  // database

  db = new Database();
  bool dbSuccess = db->Load("databases/default/database.sqlite");
  if (!dbSuccess) Log(e_FatalError, "main", "()", "Could not open database");


  // initialize systems

  SystemManager *systemManager = SystemManager::GetInstancePtr();

  graphicsSystem = new GraphicsSystem();
  bool returnvalue = systemManager->RegisterSystem("GraphicsSystem", graphicsSystem);
  if (!returnvalue) Log(e_FatalError, "football", "main", "Could not register GraphicsSystem");

  audioSystem = new AudioSystem();
  returnvalue = systemManager->RegisterSystem("AudioSystem", audioSystem);
  if (!returnvalue) Log(e_FatalError, "football", "main", "Could not register AudioSystem");

  // todo: let systemmanager init systems?
  graphicsSystem->Initialize(*config);
  audioSystem->Initialize(*config);


  // init scenes

  scene2D = boost::shared_ptr<Scene2D>(new Scene2D("scene2D", *config));
  SceneManager::GetInstance().RegisterScene(scene2D);

  scene3D = boost::shared_ptr<Scene3D>(new Scene3D("scene3D"));
  SceneManager::GetInstance().RegisterScene(scene3D);

  if (SuperDebug()) InitDebugImage();
  if (GetDebugMode() == e_DebugMode_AI) InitDebugOverlay();

  ThreadHudThread *threadHudThread = 0;
  if (!IsReleaseVersion() && 1 == 2) {
    threadHudThread = new ThreadHudThread();
    threadHudThread->Run();
  } else {
    threadHudThread = 0;
  }


  // debug pilons

  boost::intrusive_ptr < Resource<GeometryData> > geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/green.ase", true);
  greenPilon = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("greenPilon", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(greenPilon);
  greenPilon->SetGeometryData(geometry);
  greenPilon->SetLocalMode(e_LocalMode_Absolute);
  greenPilon->SetPosition(Vector3(0, 0, -10));
  //greenPilon->Disable();

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/blue.ase", true);
  bluePilon = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("bluePilon", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(bluePilon);
  bluePilon->SetGeometryData(geometry);
  bluePilon->SetLocalMode(e_LocalMode_Absolute);
  bluePilon->SetPosition(Vector3(0, 0, -10));
  //bluePilon->Disable();

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/yellow.ase", true);
  yellowPilon = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("yellowPilon", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(yellowPilon);
  yellowPilon->SetGeometryData(geometry);
  yellowPilon->SetLocalMode(e_LocalMode_Absolute);
  yellowPilon->SetPosition(Vector3(0, 0, -10));
  //yellowPilon->Disable();

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/red.ase", true);
  redPilon = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("redPilon", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(redPilon);
  redPilon->SetGeometryData(geometry);
  redPilon->SetLocalMode(e_LocalMode_Absolute);
  redPilon->SetPosition(Vector3(0, 0, -10));
  //redPilon->Disable();

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/smalldebugcircle.ase", true);
  smallDebugCircle1 = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("smallDebugCircle1", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(smallDebugCircle1);
  smallDebugCircle1->SetGeometryData(geometry);
  smallDebugCircle1->SetLocalMode(e_LocalMode_Absolute);
  smallDebugCircle1->SetPosition(Vector3(0, 0, -10));
//  smallDebugCircle1->Disable();

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/smalldebugcircle.ase", true);
  smallDebugCircle2 = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("smallDebugCircle2", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(smallDebugCircle2);
  smallDebugCircle2->SetGeometryData(geometry);
  smallDebugCircle2->SetLocalMode(e_LocalMode_Absolute);
  smallDebugCircle2->SetPosition(Vector3(0, 0, -10));
//  smallDebugCircle2->Disable();

  geometry = ResourceManagerPool::GetInstance().GetManager<GeometryData>(e_ResourceType_GeometryData)->Fetch("media/objects/helpers/largedebugcircle.ase", true);
  largeDebugCircle = static_pointer_cast<Geometry>(ObjectFactory::GetInstance().CreateObject("largeDebugCircle", e_ObjectType_Geometry));
  scene3D->CreateSystemObjects(largeDebugCircle);
  largeDebugCircle->SetGeometryData(geometry);
  largeDebugCircle->SetLocalMode(e_LocalMode_Absolute);
  largeDebugCircle->SetPosition(Vector3(0, 0, -10));
//  largeDebugCircle->Disable();

  geometry.reset();


  // --- Controllers -------------------------------------------------------------
  // Keyboard is always controller 0; any connected joysticks follow.
  HIDKeyboard *keyboard = new HIDKeyboard();
  controllers.push_back(keyboard);
  for (int i = 0; i < SDL_NumJoysticks(); i++) {
    HIDGamepad *gamepad = new HIDGamepad(i);
    controllers.push_back(gamepad);
  }


  // --- Tasks & scheduler sequences --------------------------------------------
  // The scheduler runs two concurrent TaskSequences:
  //   gameSequence    — fixed-timestep game logic (menu + gameplay)
  //   graphicsSequence — uncapped render loop (reads game state written by gameTask::Put)
  //
  // Each task has three phases executed in order: Get (read input/state),
  // Process (update), Put (write output/scene data).

  boost::mutex graphicsGameMutex; // guards scene writes vs. reads across sequences

  gameTask = boost::shared_ptr<GameTask>(new GameTask());

  std::string fontfilename = config->Get("font_filename", "media/fonts/alegreya/AlegreyaSansSC-ExtraBold.ttf");
  TTF_Font *defaultFont = TTF_OpenFont(fontfilename.c_str(), 32);
  if (!defaultFont) Log(e_FatalError, "football", "main", "Could not load font " + fontfilename);
  TTF_Font *defaultOutlineFont = TTF_OpenFont(fontfilename.c_str(), 32);
  TTF_SetFontOutline(defaultOutlineFont, 2); // outline variant used for legible text over the pitch
  menuTask = boost::shared_ptr<MenuTask>(new MenuTask(5.0f / 4.0f, 0, defaultFont, defaultOutlineFont));
  // Wire the first gamepad's A/B buttons to the menu confirm/back actions.
  if (controllers.size() > 1) menuTask->SetEventJoyButtons(static_cast<HIDGamepad*>(controllers.at(1))->GetControllerMapping(e_ControllerButton_A), static_cast<HIDGamepad*>(controllers.at(1))->GetControllerMapping(e_ControllerButton_B));


  // Game sequence: runs at the fixed physics rate.
  // Order matters — menu runs before gameplay so UI input is consumed first.
  gameSequence = boost::shared_ptr<TaskSequence>(new TaskSequence("game", timeStep_ms, false));
  gameSequence->AddUserTaskEntry(menuTask, e_TaskPhase_Get);
  gameSequence->AddUserTaskEntry(menuTask, e_TaskPhase_Process);
  gameSequence->AddUserTaskEntry(menuTask, e_TaskPhase_Put);
  gameSequence->AddUserTaskEntry(gameTask, e_TaskPhase_Get);
  gameSequence->AddUserTaskEntry(gameTask, e_TaskPhase_Process);
  GetScheduler()->RegisterTaskSequence(gameSequence);

  // Graphics sequence: runs as fast as possible (frametime_ms = 0 means uncapped).
  // gameTask::Put flushes scene-object positions so the renderer sees consistent state.
  graphicsSequence = boost::shared_ptr<TaskSequence>(new TaskSequence("graphics", config->GetInt("graphics3d_frametime_ms", 0), true));
  graphicsSequence->AddUserTaskEntry(gameTask, e_TaskPhase_Put);
  graphicsSequence->AddSystemTaskEntry(graphicsSystem, e_TaskPhase_Get);
  graphicsSequence->AddSystemTaskEntry(graphicsSystem, e_TaskPhase_Process);
  graphicsSequence->AddSystemTaskEntry(graphicsSystem, e_TaskPhase_Put);
  GetScheduler()->RegisterTaskSequence(graphicsSequence);


  // --- Run --------------------------------------------------------------------
#ifdef __APPLE__
  // macOS requires OpenGL and Cocoa event pumping on the main thread.
  // The scheduler (game logic) moves to a background thread; the main thread
  // drives RunMainLoop() which blocks until the game exits.
  {
    boost::thread schedulerThread([]() { Run(); });
    graphicsSystem->RunMainLoop();
    schedulerThread.join();
  }
#else
  Run(); // blocks until the scheduler stops
#endif


  // --- Shutdown ---------------------------------------------------------------
  // Release resources in reverse-construction order.

  if (SuperDebug()) scene2D->DeleteObject(debugImage);
  if (GetDebugMode() == e_DebugMode_AI) scene2D->DeleteObject(debugOverlay);

  gameTask.reset();
  menuTask.reset();

  gameSequence.reset();
  graphicsSequence.reset();

  greenPilon->Exit();
  greenPilon.reset();
  bluePilon->Exit();
  bluePilon.reset();
  yellowPilon->Exit();
  yellowPilon.reset();
  redPilon->Exit();
  redPilon.reset();
  smallDebugCircle1->Exit();
  smallDebugCircle1.reset();
  smallDebugCircle2->Exit();
  smallDebugCircle2.reset();
  largeDebugCircle->Exit();
  largeDebugCircle.reset();

  if (threadHudThread) {
    boost::intrusive_ptr<Message_Shutdown> shutdownMessage = new Message_Shutdown();
    threadHudThread->messageQueue.PushMessage(shutdownMessage);
    threadHudThread->Join();
    delete threadHudThread;
    shutdownMessage.reset();
  }

  scene2D.reset();
  scene3D.reset();

  for (unsigned int i = 0; i < controllers.size(); i++) {
    delete controllers.at(i);
  }
  controllers.clear();

  TTF_CloseFont(defaultFont); // todo: better timed closefont?
  TTF_CloseFont(defaultOutlineFont); // todo: better timed closefont?

  delete db;
  delete config;

  Exit();

  return 0;
}

