// written by bastiaan konings schuiling 2008 - 2014
// this work is public domain. the code is undocumented, scruffy, untested, and should generally not be used for anything important.
// i do not offer support, so don't ask. to be used for inspiration :)

#include "graphics_system.hpp"

#include "graphics_scene.hpp"

#include "base/log.hpp"
#include "base/utils.hpp"

#include "managers/resourcemanagerpool.hpp"

#include "rendering/r3d_messages.hpp"

namespace blunted {

  GraphicsSystem::GraphicsSystem() : systemType(e_SystemType_Graphics) {
    renderer3DTask = NULL;
    task = NULL;
  }

  GraphicsSystem::~GraphicsSystem() {
  }

  void GraphicsSystem::Initialize(const Properties &config) {

    textureResourceManager = boost::shared_ptr < ResourceManager<Texture> > (new ResourceManager<Texture>("texture"));
    vertexBufferResourceManager = boost::shared_ptr < ResourceManager<VertexBuffer> > (new ResourceManager<VertexBuffer>("vertexbuffer"));
    ResourceManagerPool::GetInstance().RegisterManager(e_ResourceType_Texture, textureResourceManager);
    ResourceManagerPool::GetInstance().RegisterManager(e_ResourceType_VertexBuffer, vertexBufferResourceManager);

    if (config.Get("graphics3d_renderer", "opengl") == "opengl") renderer3DTask = new OpenGLRenderer3D();
    width = config.GetInt("context_x", 1280);
    height = config.GetInt("context_y", 720);
    bpp = config.GetInt("context_bpp", 32);
    bool fullscreen = config.GetBool("context_fullscreen", false);

#ifdef __APPLE__
    // On macOS the SDL video subsystem and the OpenGL context must be created on
    // the process main thread (Cocoa / NSApp requirement).  We therefore call
    // InitSDL() and CreateContext() directly here rather than via the renderer
    // thread's message queue.  RunMainLoop() must be called from main() to drive
    // the event pump and process renderer commands on the main thread.
    OpenGLRenderer3D *glRenderer = static_cast<OpenGLRenderer3D*>(renderer3DTask);
    glRenderer->InitSDL();
    bool ctxOK = glRenderer->CreateContext(width, height, bpp, fullscreen);
    if (!ctxOK) {
      Log(e_FatalError, "GraphicsSystem", "Initialize", "Could not create context");
    } else {
      Log(e_Notice, "GraphicsSystem", "Initialize", "Created context, resolution " + int_to_str(width) + " * " + int_to_str(height) + " @ " + int_to_str(bpp) + " bpp");
    }
    // The window + GL context were created above on the process main thread
    // (Cocoa requirement) and the context was released by CreateContext.  Now
    // start the renderer on its own thread; it claims the context and drains
    // its message queue.  Starting it here (rather than deferring all GL to the
    // main thread) is essential: the main thread blocks on the renderer queue
    // during scene/resource setup, so the renderer must already be running to
    // service those requests.  SDL window events are pumped on the main thread
    // via RunMainLoop().
    renderer3DTask->Run();
#else
    // Linux / Windows: renderer runs in its own thread.
    renderer3DTask->Run();

    boost::intrusive_ptr<Renderer3DMessage_CreateContext> createContext(new Renderer3DMessage_CreateContext(width, height, bpp, fullscreen));
    renderer3DTask->messageQueue.PushMessage(createContext);
    createContext->Wait();

    if (!createContext->success) {
      Log(e_FatalError, "GraphicsSystem", "Initialize", "Could not create context");
    } else {
      Log(e_Notice, "GraphicsSystem", "Initialize", "Created context, resolution " + int_to_str(width) + " * " + int_to_str(height) + " @ " + int_to_str(bpp) + " bpp");
    }
#endif

    task = new GraphicsTask(this);
    task->Run();
  }

  void GraphicsSystem::Exit() {
    // Shutdown the GraphicsTask thread (same on all platforms).
    boost::intrusive_ptr<Message_Shutdown> shutdown(new Message_Shutdown());
    task->messageQueue.PushMessage(shutdown);
    shutdown->Wait();
    task->Join();
    delete task;
    task = NULL;

    textureResourceManager.reset();
    vertexBufferResourceManager.reset();

    // Stop the renderer thread (on all platforms it now runs on its own thread).
    boost::intrusive_ptr<Message_Shutdown> R3Dshutdown(new Message_Shutdown());
    renderer3DTask->messageQueue.PushMessage(R3Dshutdown);
    R3Dshutdown->Wait();
    renderer3DTask->Join();
    delete renderer3DTask;
    renderer3DTask = NULL;
  }

  void GraphicsSystem::RunMainLoop() {
#ifdef __APPLE__
    renderer3DTask->RunLoop();
#endif
  }

  e_SystemType GraphicsSystem::GetSystemType() const {
    return systemType;
  }

  ISystemScene *GraphicsSystem::CreateSystemScene(boost::shared_ptr<IScene> scene) {
    if (scene->GetSceneType() == e_SceneType_Scene2D) {
      GraphicsScene *graphicsScene = new GraphicsScene(this);
      scene->Attach(graphicsScene->GetInterpreter(e_SceneType_Scene2D));
      return graphicsScene;
    }
    if (scene->GetSceneType() == e_SceneType_Scene3D) {
      GraphicsScene *graphicsScene = new GraphicsScene(this);
      scene->Attach(graphicsScene->GetInterpreter(e_SceneType_Scene3D));
      return graphicsScene;
    }
    return NULL;
  }

  ISystemTask *GraphicsSystem::GetTask() {
    return task;
  }

  Renderer3D *GraphicsSystem::GetRenderer3D() {
    return renderer3DTask;
  }

  MessageQueue<Overlay2DQueueEntry> &GraphicsSystem::GetOverlay2DQueue() {
    return overlay2DQueue;
  }

}
