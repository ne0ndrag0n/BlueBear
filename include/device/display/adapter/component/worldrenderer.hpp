#ifndef WORLD_RENDERER
#define WORLD_RENDERER

#include "device/display/adapter/adapter.hpp"
#include "containers/vec3map.hpp"
#include "graphics/camera.hpp"
#include "graphics/scenegraph/resourcebank.hpp"
#include <tbb/concurrent_unordered_map.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <sol.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

namespace BlueBear {
  namespace Graphics {
    class Shader;

    namespace SceneGraph {
      class Model;
      class Light;

      namespace ModelLoader {
        class FileModelLoader;
      }
    }
  }

  namespace Device {
    namespace Display {
      class Display;

      namespace Adapter {
        namespace Component {

          class WorldRenderer : public Adapter {
            Graphics::Camera camera;

            Graphics::SceneGraph::ResourceBank cache;
            std::unordered_map< std::string, std::shared_ptr< Graphics::SceneGraph::Model > > originals;

            // TODO: Walls and floor which both consist of one generated model
            std::unordered_map< std::string, std::shared_ptr< Graphics::SceneGraph::Light > > lights;
            std::unordered_map< std::string, std::shared_ptr< Graphics::SceneGraph::Model > > models;

            std::unique_ptr< Graphics::SceneGraph::ModelLoader::FileModelLoader > getFileModelLoader( bool deferGLOperations );

          public:
            WorldRenderer( Display& display );
            virtual ~WorldRenderer();

            void submitLuaContributions( sol::state& lua );

            std::shared_ptr< Graphics::SceneGraph::Model > placeObject( const std::string& objectId, const std::string& newId );
            std::shared_ptr< Graphics::SceneGraph::Model > getObject( const std::string& instanceId );

            Graphics::Camera& getCamera();
            void loadPathsParallel( const std::vector< std::pair< std::string, std::string > >& paths );
            void loadPaths( const std::vector< std::pair< std::string, std::string > >& paths );
            void nextFrame() override;
          };

        }
      }
    }
  }

}

#endif
