#include "graphics/display.hpp"
#include "containers/collection3d.hpp"
#include "containers/conccollection3d.hpp"
#include "graphics/imagebuilder/imagesource.hpp"
#include "graphics/imagebuilder/pathimagesource.hpp"
#include "graphics/instance/instance.hpp"
#include "graphics/shader.hpp"
#include "graphics/model.hpp"
#include "graphics/drawable.hpp"
#include "graphics/camera.hpp"
#include "graphics/material.hpp"
#include "graphics/texture.hpp"
#include "graphics/wallcellbundler.hpp"
#include "graphics/widgetbuilder.hpp"
#include "scripting/lot.hpp"
#include "scripting/tile.hpp"
#include "scripting/engine.hpp"
#include "scripting/wallcell.hpp"
#include "scripting/wallpaper.hpp"
#include "threading/commandbus.hpp"
#include "threading/lockable.hpp"
#include "localemanager.hpp"
#include "configmanager.hpp"
#include "log.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/OpenGL.hpp>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <cstdlib>
#include <utility>
#include <functional>

static BlueBear::Graphics::Display::CommandList displayCommandList;
static BlueBear::Scripting::Engine::CommandList engineCommandList;

namespace BlueBear {
  namespace Graphics {

    const std::string Display::WALLPANEL_MODEL_XY_PATH = "system/models/wall/wall_new.dae";
    const std::string Display::WALLPANEL_MODEL_DR_PATH = "system/models/wall/diagwall.dae";
    const std::string Display::FLOOR_MODEL_PATH = "system/models/floor/floor.dae";

    Display::Display( Threading::CommandBus& commandBus ) : commandBus( commandBus ) {
      // Get our settings out of the config manager
      x = ConfigManager::getInstance().getIntValue( "viewport_x" );
      y = ConfigManager::getInstance().getIntValue( "viewport_y" );

      // There must always be a defined State (avoid branch penalty/null check in tight loop)
      currentState = std::make_unique< IdleState >( *this );
    }

    Display::~Display() = default;

    void Display::openDisplay() {
      mainWindow.create( sf::VideoMode( x, y ), LocaleManager::getInstance().getString( "BLUEBEAR_WINDOW_TITLE" ), sf::Style::Close, sf::ContextSettings( 24, 8, 0, 3, 3 ) );

      // Set sync on window by these params:
      // vsync_limiter_overview = true or fps_overview
      if( ConfigManager::getInstance().getBoolValue( "vsync_limiter_overview" ) == true ) {
        mainWindow.setVerticalSyncEnabled( true );
      } else {
        mainWindow.setFramerateLimit( ConfigManager::getInstance().getIntValue( "fps_overview" ) );
      }

      // Initialize OpenGL using GLEW
      glewExperimental = true;
      auto glewStatus = glewInit();
      if( glewStatus != GLEW_OK ) {
        // Oh, this piece of shit function. Why the fuck is this GLubyte* when every other god damn string type is const char*?!
        Log::getInstance().error( "Display::openDisplay", "FATAL: glewInit() did NOT return GLEW_OK! (" + std::string( ( const char* ) glewGetErrorString( glewStatus ) ) + ")" );
        exit( 1 );
      }

      // There may be more than just this needed from main.cpp in the area51/sfml_test project
      glViewport( 0, 0, x, y );
      glEnable( GL_DEPTH_TEST );
      glEnable( GL_CULL_FACE );
    }

    void Display::start() {
      while( mainWindow.isOpen() ) {
        // Process incoming commands - the passed-in list should always be empty
        commandBus.attemptConsume( displayCommandList );

        for( auto& command : displayCommandList ) {
          command->execute( *this );
        }

        displayCommandList.clear();

        // Handle rendering
        currentState->execute();

        // Handle events
        sf::Event event;
        while( mainWindow.pollEvent( event ) ) {
          // This might be all we need for now
          switch( event.type ) {
            case sf::Event::Closed:
              mainWindow.close();
              break;
            default:
              // Any event not handled by Display itself
              // is passed along to currentState
              currentState->handleEvent( event );
              break;
          }
        }

        // Process outgoing commands
        if( engineCommandList.size() > 0 ) {
          commandBus.attemptProduce( engineCommandList );
        }
      }
    }

    /**
     * Given a lot, build floorInstanceCollection and translate the Tiles/Wallpanels to instances on the lot. Additionally, send the rotation status.
     */
    void Display::loadInfrastructure( unsigned int currentRotation, Containers::ConcCollection3D< Threading::Lockable< Scripting::Tile > >& floorMap, Containers::ConcCollection3D< Threading::Lockable< Scripting::WallCell > >& wallMap ) {

      std::unique_ptr< Display::MainGameState > mainGameStatePtr = std::make_unique< Display::MainGameState >( *this, currentRotation, floorMap, wallMap );

      currentState = std::move( mainGameStatePtr );

      // Drop a SetLockState command for Engine
      engineCommandList.push_back( std::make_unique< Scripting::Engine::SetLockState >( true ) );
    }

    // ---------- STATES ----------

    Display::State::State( Display& instance ) : instance( instance ) {}

    // Does nothing. This is better than a null pointer check in a tight loop.
    Display::IdleState::IdleState( Display& instance ) : Display::State::State( instance ) {}
    void Display::IdleState::handleEvent( sf::Event& event ) {}
    void Display::IdleState::execute() {
      glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
      glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

      instance.mainWindow.display();
    }

    /**
     * Display renderer state for the titlescreen
     */
    Display::TitleState::TitleState( Display& instance ) : Display::State::State( instance ) {}
    void Display::TitleState::handleEvent( sf::Event& event ) {}
    void Display::TitleState::execute() {
      glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
      glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

      instance.mainWindow.display();
    }

    /**
     * Display renderer state for the main game loop
     */
    Display::MainGameState::MainGameState( Display& instance, unsigned int currentRotation, Containers::ConcCollection3D< Threading::Lockable< Scripting::Tile > >& floorMap, Containers::ConcCollection3D< Threading::Lockable< Scripting::WallCell > >& wallMap ) :
      Display::State::State( instance ),
      defaultShader( Shader( "system/shaders/default_vertex.glsl", "system/shaders/default_fragment.glsl" ) ),
      camera( Camera( defaultShader.Program, instance.x, instance.y ) ),
      floorModel( Model( Display::FLOOR_MODEL_PATH ) ),
      floorMap( floorMap ),
      wallMap( wallMap ),
      currentRotation( currentRotation ) {

      // Load infrastructure models
      // Setup static pointer
      WallCellBundler::Piece = std::make_unique< Model >( Display::WALLPANEL_MODEL_XY_PATH );
      WallCellBundler::DPiece = std::make_unique< Model >( Display::WALLPANEL_MODEL_DR_PATH );

      // Setup camera
      camera.setRotationDirect( currentRotation );

      try {
        setupDefaultWindows();
      } catch( ... ) {
        Log::getInstance().error(
          "Display::MainGameState::MainGameState",
          "Failed to create debug panel!"
        );
      }

      // Moving much of Display::loadInfrastructure here
      loadInfrastructure();
    }
    Display::MainGameState::~MainGameState() {
      WallCellBundler::Piece.reset();
      WallCellBundler::DPiece.reset();
    }
    void Display::MainGameState::setupDefaultWindows() {
      WidgetBuilder builder( "system/ui/main/debug/debug.xml" );
      std::vector< std::shared_ptr< sfg::Widget > > widgets = builder.getWidgets();

      for( std::shared_ptr< sfg::Widget > widget : widgets ) {
        gui.desktop.Add( widget );
      }

      if( !gui.desktop.LoadThemeFromFile( ConfigManager::getInstance().getValue( "ui_theme" ) ) ) {
        Log::getInstance().warn( "Display::MainGameState::MainGameState", "ui_theme unable to load." );
      }
    }
    void Display::MainGameState::createFloorInstances() {
      floorInstanceCollection->clear();

      auto dimensions = floorMap.getDimensions();

      float xOrigin = -( (int)dimensions.x / 2 ) + 0.5f;
      float yOrigin = ( dimensions.y / 2 ) - 0.5f;

      for ( unsigned int zCounter = 0; zCounter != dimensions.levels; zCounter++ ) {
        for( unsigned int yCounter = 0; yCounter != dimensions.y; yCounter++ ) {
          for( unsigned int xCounter = 0; xCounter != dimensions.x; xCounter++ ) {

            glm::vec3 floorCoords( xOrigin + xCounter, yOrigin - yCounter, zCounter * 2.0f );

            Threading::Lockable< Scripting::Tile > tilePtr = floorMap.getItem( zCounter, xCounter, yCounter );

            if( tilePtr ) {
              // Create instance from the model, and change its material using the material cache
              std::shared_ptr< Instance > instance = std::make_shared< Instance >( floorModel, defaultShader.Program );

              Drawable& floorDrawable = *( instance->drawable );

              tilePtr.lock( [ & ]( Scripting::Tile& tile ) {
                floorDrawable.material = std::make_shared< Material >( TextureList{ texCache.get( tile.imagePath ) } );
              } );

              instance->setPosition( floorCoords );

              // The pointer to this floor tile goes into the floorInstanceCollection
              floorInstanceCollection->pushDirect( instance );
            } else {
              // There is no floor tile located here. Consequently, insert an empty Instance pointer here; it will be skipped on draw.
              floorInstanceCollection->pushDirect( std::shared_ptr< Instance >() );
            }

          }
        }
      }
    }
    void Display::MainGameState::createWallInstances() {
      wallInstanceCollection->clear();

      auto dimensions = wallMap.getDimensions();

      WallCellBundler::xOrigin = -( (int)dimensions.x / 2 ) + 0.5f;
      WallCellBundler::yOrigin = ( dimensions.y / 2 ) - 0.5f;

      for ( unsigned int zCounter = 0; zCounter != dimensions.levels; zCounter++ ) {
        for( unsigned int yCounter = 0; yCounter != dimensions.y; yCounter++ ) {
          for( unsigned int xCounter = 0; xCounter != dimensions.x; xCounter++ ) {

            Threading::Lockable< Scripting::WallCell > wallCellPtr = wallMap.getItem( zCounter, xCounter, yCounter );
            std::shared_ptr< WallCellBundler > wallCellBundler;
            if( wallCellPtr ) {
              // Several different kinds of wall panel models depending on the type, and several kinds of orientations
              // If that pointer exists, at least one of these ifs will be fulfilled
               wallCellBundler = std::make_shared< WallCellBundler >(
                 wallCellPtr,
                 *wallInstanceCollection,
                 glm::vec3( xCounter, yCounter, zCounter ),
                 currentRotation, texCache, imageCache, defaultShader.Program
               );
            }

            wallInstanceCollection->pushDirect( wallCellBundler );

          }
        }
      }
    }
    void Display::MainGameState::loadInfrastructure() {
      auto dimensions = floorMap.getDimensions();
      floorInstanceCollection = std::make_unique< Containers::Collection3D< std::shared_ptr< Instance > > >( dimensions.levels, dimensions.x, dimensions.y );

      auto dimensionsWall = wallMap.getDimensions();
      wallInstanceCollection = std::make_unique< Containers::Collection3D< std::shared_ptr< WallCellBundler > > >( dimensionsWall.levels, dimensionsWall.x, dimensionsWall.y );

      createFloorInstances();
      createWallInstances();

      Log::getInstance().info( "Display::MainGameState::loadInfrastructure", "Finished creating infrastructure instances." );
    }
    void Display::MainGameState::execute() {
      glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
      glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

      // Use default shader and position camera
      defaultShader.use();
      camera.position();

      // Draw entities of each type
      // Floor & Walls with nudging
      auto length = floorInstanceCollection->getLength();
      for( auto i = 0; i != length; i++ ) {
        std::shared_ptr< Instance > floorInstance = floorInstanceCollection->getItemDirect( i );

        if( floorInstance ) {
          floorInstance->drawEntity();
        }
      }

      auto wallLength = wallInstanceCollection->getLength();
      for( auto i = 0; i != wallLength; i++ ) {
        std::shared_ptr< WallCellBundler > wallCellBundler = wallInstanceCollection->getItemDirect( i );

        if( wallCellBundler ) {
          wallCellBundler->render();
        }
      }

      instance.mainWindow.pushGLStates();
        processOsd();
      instance.mainWindow.popGLStates();

      instance.mainWindow.display();
    }
    void Display::MainGameState::handleEvent( sf::Event& event ) {
      // Lay out some statics
      static int KEY_PERSPECTIVE = ConfigManager::getInstance().getIntValue( "key_switch_perspective" );
      static int KEY_ROTATE_RIGHT = ConfigManager::getInstance().getIntValue( "key_rotate_right" );
      static int KEY_ROTATE_LEFT = ConfigManager::getInstance().getIntValue( "key_rotate_left" );
      static int KEY_UP = ConfigManager::getInstance().getIntValue( "key_move_up" );
      static int KEY_DOWN = ConfigManager::getInstance().getIntValue( "key_move_down" );
      static int KEY_LEFT = ConfigManager::getInstance().getIntValue( "key_move_left" );
      static int KEY_RIGHT = ConfigManager::getInstance().getIntValue( "key_move_right" );
      static int KEY_ZOOM_IN = ConfigManager::getInstance().getIntValue( "key_zoom_in" );
      static int KEY_ZOOM_OUT = ConfigManager::getInstance().getIntValue( "key_zoom_out" );

      gui.desktop.HandleEvent( event );

      // Main game has a few events mostly relating to camera
      switch( event.type ) {
        case sf::Event::KeyPressed:
          // absolutely disgusting
          {
            auto keyCode = event.key.code;

            /*
            if( keyCode == KEY_PERSPECTIVE ) {
              if( camera.ortho ) {
                camera.setOrthographic( false );
                //instance.mainWindow.setMouseCursorVisible( false );
                // there is no center yet
                //sf::Mouse::setPosition( center, instance.mainWindow );
                // just forget about this for now
                //instance.mainWindow.setFramerateLimit( 60 );
              } else {
                camera.setOrthographic( true );
                //instance.mainWindow.setMouseCursorVisible( true );
                // just forget about this for now
                //instance.mainWindow.setFramerateLimit( 30 );
              }
            }
            */

            if( keyCode == KEY_ROTATE_RIGHT ) {
              currentRotation = camera.rotateRight();
              createWallInstances();
            }

            if( keyCode == KEY_ROTATE_LEFT ) {
              currentRotation = camera.rotateLeft();
              createWallInstances();
            }

            if( keyCode == KEY_UP ) {
              camera.move( 0.0f, -0.1f, 0.0f );
            }
            if( keyCode == KEY_DOWN ) {
              camera.move( 0.0f, 0.1f, 0.0f );
            }
            if( keyCode == KEY_LEFT ) {
              camera.move( 0.1f, 0.0f, 0.0f );
            }
            if( keyCode == KEY_RIGHT ) {
              camera.move( -0.1f, 0.0f, 0.0f );
            }
            if( keyCode == KEY_ZOOM_IN ) {
              camera.zoomIn();
            }
            if( keyCode == KEY_ZOOM_OUT ) {
              camera.zoomOut();
            }

            break;
          }
        default:
          break;
      }
    }
    void Display::MainGameState::processOsd() {
      gui.desktop.Update( gui.clock.restart().asSeconds() );
      instance.sfgui.Display( instance.mainWindow );
    }

    // ---------- COMMANDS ----------
    void Display::NewEntityCommand::execute( Graphics::Display& instance ) {
      Log::getInstance().info( "NewEntityCommand", "Called registerNewEntity, hang in there..." );
    }

    Display::SendInfrastructureCommand::SendInfrastructureCommand( Scripting::Lot& lot ) :
      rotation( lot.currentRotation ),
      floorMap( *lot.floorMap ),
      wallMap( *lot.wallMap ) {}
    void Display::SendInfrastructureCommand::execute( Graphics::Display& instance ) {
      instance.loadInfrastructure( rotation, floorMap, wallMap );
    }
  }
}
