#include "graphics/scenegraph/transform.hpp"
#include "tools/opengl.hpp"
#include <glm/ext.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GL/glew.h>

namespace BlueBear {
  namespace Graphics {
    namespace SceneGraph {

      Transform::Transform() {}

      Transform::Transform( const glm::vec3& position, const glm::vec3& scale, const glm::quat& rotation ) :
        position( position ), scale( scale ), rotation( rotation ) {}

      Transform::Transform( const glm::mat4& existingTransform ) : result( existingTransform ), dirty( false ) {}

      Transform::Transform( const Transform& transform ) : position( transform.position ), scale( transform.scale ), rotation( transform.rotation ), result( transform.result ), dirty( transform.dirty ) {}

      Transform& Transform::operator=( const Transform& other ) {
        position = other.position;
        scale = other.scale;
        rotation = other.rotation;
        result = other.result;
        dirty = other.dirty;
      }

      void Transform::recalculate() {
        if( dirty ) {
          result = glm::mat4( 1.0f );
          result *= glm::translate( position );
          result *= glm::toMat4( rotation );
          result *= glm::scale( scale );
          dirty = false;
        }
      }

      glm::mat4 Transform::getMatrix() {
        recalculate();

        return result;
      }

      glm::vec3 Transform::getPosition() {
        return position;
      }

      void Transform::setPosition( const glm::vec3& position ) {
        this->position = position;
        dirty = true;
      }

      glm::vec3 Transform::getScale() {
        return scale;
      }

      void Transform::setScale( const glm::vec3& scale ) {
        this->scale = scale;
        dirty = true;
      }

      glm::quat Transform::getRotation() {
        return rotation;
      }

      void Transform::setRotation( const glm::quat& rotation ) {
        this->rotation = rotation;
        dirty = true;
      }

      void Transform::send( const glm::mat4& parentMixin ) {
        recalculate();

        glm::mat4 quickMatrix = parentMixin * result;
        glUniformMatrix4fv( Tools::OpenGL::getUniformLocation( "model" ), 1, GL_FALSE, glm::value_ptr( quickMatrix ) );
      }

      Transform Transform::interpolate( Transform& t1, Transform& t2, float alpha ) {
        return Transform(
          glm::mix( t1.getPosition(), t2.getPosition(), alpha ),
          glm::mix( t1.getScale(), t2.getScale(), alpha ),
          glm::mix( t1.getRotation(), t2.getRotation(), alpha )
        );
      }

      glm::mat4 Transform::componentsToMatrix( const glm::vec3& position, const glm::vec3& scale, const glm::quat& rotation ) {
        glm::mat4 quickMatrix;

        quickMatrix = glm::translate( quickMatrix, position );
        quickMatrix = quickMatrix * glm::toMat4( rotation );
        quickMatrix = glm::scale( quickMatrix, scale );

        return quickMatrix;
      }

    }
  }
}