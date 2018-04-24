#include "graphics/userinterface/widgets/input.hpp"
#include "device/display/adapter/component/guicomponent.hpp"
#include <glm/glm.hpp>

namespace BlueBear::Graphics::UserInterface::Widgets {

  Input::Input( const std::string& id, const std::vector< std::string >& classes, const std::string& hintText, const std::string& contents ) :
    Element::Element( "Input", id, classes ), hintText( hintText ), contents( contents ) {}

  std::shared_ptr< Input > Input::create( const std::string& id, const std::vector< std::string >& classes, const std::string& hintText, const std::string& contents ) {
    std::shared_ptr< Input > input( new Input( id, classes, hintText, contents ) );

    return input;
  }

  void Input::render( Graphics::Vector::Renderer& renderer ) {

    renderer.drawRect(
      { 0, 0, allocation[ 2 ], allocation[ 3 ] },
      { 255, 0, 255, 128 }
    );

    // Text
    double fontSize = localStyle.get< double >( "font-size" );
    renderer.drawText(
      localStyle.get< std::string >( "font" ),
      hintText,
      { 4, allocation[ 3 ] - fontSize - 3 },
      localStyle.get< glm::uvec4 >( "font-hint-color" ),
      fontSize
    );

    // Line
    renderer.drawRect(
      { 4, allocation[ 3 ] - 12, allocation[ 2 ] - 4, allocation[ 3 ] - 8 },
      localStyle.get< glm::uvec4 >( "color" )
    );
  }

  void Input::calculate() {
    double fontSize = localStyle.get< double >( "font-size" );
    glm::vec4 size = manager->getVectorRenderer().getTextSizeParams( localStyle.get< std::string >( "font" ), hintText, fontSize );
    textSpan = size[ 2 ];

    requisition = glm::uvec2{ textSpan + 16, fontSize + 20 };
  }

  void Input::reflow() {
    if( auto parent = getParent() ) {
      parent->reflow();
    } else {
      Element::reflow();
    }
  }

}