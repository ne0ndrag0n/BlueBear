#include "graphics/scenegraph/light/sector_illuminator.hpp"
#include "graphics/texture.hpp"
#include "tools/opengl.hpp"
#include "configmanager.hpp"
#include "log.hpp"
#include <algorithm>
#include <limits>
#include <chrono>
#include <glm/gtx/string_cast.hpp>
#include <tbb/parallel_for.h>
#include <tbb/parallel_for_each.h>

namespace BlueBear::Graphics::SceneGraph::Light {

	SectorIlluminator::~SectorIlluminator() {
		for( const auto& pair : textureData ) {
			if( pair.textureUnit ) {
				Tools::OpenGL::returnTextureUnits( { *pair.textureUnit } );
			}
		}
	}

	static glm::vec3 correctByOrigin( const glm::vec3& value, const glm::vec3& origin ) {
		return { origin.x + value.x, origin.y - value.y, value.z };
	}

	static bool segmentsIntersect( const std::pair< glm::vec3, glm::vec3 >& line1, const std::pair< glm::vec3, glm::vec3 >& line2 ) {
		// ta = (y3−y4)(x1−x3)+(x4−x3)(y1−y3)
		//      -----------------------------
		//      (x4−x3)(y1−y2)−(x1−x2)(y4−y3)

		// tb = (y1−y2)(x1−x3)+(x2−x1)(y1−y3)
		//      -----------------------------
		//      (x4−x3)(y1−y2)−(x1−x2)(y4−y3)

		// first - odd
		// second - even

		float denominator = ( ( line2.second.x - line2.first.x ) * ( line1.first.y - line1.second.y ) ) -
							( ( line1.first.x - line1.second.x ) * ( line2.second.y - line2.first.y ) );

		// collinear
		if( denominator == 0.0f ) {
			return false;
		}

	float ta_numerator = ( ( line2.first.y - line2.second.y ) * ( line1.first.x - line2.first.x ) ) +
							( ( line2.second.x - line2.first.x ) * ( line1.first.y - line2.first.y ) );

		float tb_numerator = ( ( line1.first.y - line1.second.y ) * ( line1.first.x - line2.first.x ) ) +
							( ( line1.second.x - line1.first.x ) * ( line1.first.y - line2.first.y ) );


		float ta = ta_numerator / denominator;
		float tb = tb_numerator / denominator;

		return ( ta >= 0.0f && ta <= 1.0f && tb >= 0.0f && tb <= 1.0f );
	}

	static float getPolygonMaxX( const SectorIlluminator::Sector& sector, const glm::vec3& origin ) {
		float maxX = 1.175494351e-38;

		for( const auto& lineSegment : sector.sides ) {
			maxX = std::max(
				maxX,
				std::max(
					correctByOrigin( lineSegment.first, origin ).x,
					correctByOrigin( lineSegment.second, origin ).x
				)
			);
		}

		return maxX;
	}

	void SectorIlluminator::send() {
		if( dirty ) {
			refresh();

			// Texture units 1-9 are reserved for levels.
			if( textureData.size() > 8 ) {
				Log::getInstance().warn( "SectorIlluminator::send", "Cannot send > 8 sector maps to shader!" );
			}
		}

		int resolution = std::min( ( int ) std::pow( 10, ConfigManager::getInstance().getIntValue( "sector_resolution" ) ), 100 );
		Tools::OpenGL::setUniform( "sectorResolution", ( float ) resolution );

		int item = 0;
		for( const auto& pair : levelData ) {
			Tools::OpenGL::setUniform( "sectors[" + std::to_string( item ) + "].origin", pair.first );
			Tools::OpenGL::setUniform( "sectors[" + std::to_string( item ) + "].dimensions", pair.second );

			item++;
			if( item == 8 ) {
				break;
			}
		}

		item = 0;
		for( auto& pair : textureData ) {
			if( !pair.textureUnit ) {
				auto potential = Tools::OpenGL::getTextureUnit();
				if( !potential ) {
					Log::getInstance().error( "SectorIlluminator::send", "Couldn't get texure unit; not sending texture to shader." );
					continue;
				}

				pair.textureUnit = *potential;
			}

			if( pair.generator.valid() ) {
				Log::getInstance().debug( "SectorIlluminator::send", "Regenerating sector texture for level " + std::to_string( item ) );
				pair.texture = pair.generator.get();
			}

			if( !pair.texture ) {
				Log::getInstance().error( "SectorIlluminator::send", "Assertion failed! Texture wasn't generated for a sector." );
				continue;
			}

			glActiveTexture( GL_TEXTURE0 + *pair.textureUnit );
			glBindTexture( GL_TEXTURE_2D, pair.texture->id );
			Tools::OpenGL::setUniform( "sectorMap" + std::to_string( item ), ( int ) *pair.textureUnit );

			item++;
			if( item == 8 ) {
				break;
			}
		}

		item = 0;
		for( const Sector& sector : sectors ) {
			Tools::OpenGL::setUniform( "sectorLights[" + std::to_string( item ) + "].direction", sector.direction );
			Tools::OpenGL::setUniform( "sectorLights[" + std::to_string( item ) + "].ambient", sector.ambient );
			Tools::OpenGL::setUniform( "sectorLights[" + std::to_string( item ) + "].diffuse", sector.diffuse );
			Tools::OpenGL::setUniform( "sectorLights[" + std::to_string( item ) + "].specular", sector.specular );

			item++;
		}
	}

	/**
	 * Generate and save anything that isn't trivial to send
	 */
	void SectorIlluminator::refresh() {
		if( generatorTask.valid() ) {
			// Future is staged and is in-progress or complete
			// Check if future is ready
			if( generatorTask.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready ) {
				// If elements were staged, we tried to add something while another operation was in progress
				// Discard the results of that and stage a new operation
				if( staging.sectors.size() || staging.levelData.size() ) {

					sectors.insert( sectors.end(), staging.sectors.begin(), staging.sectors.end() );
					levelData.insert( levelData.end(), staging.levelData.begin(), staging.levelData.end() );

					staging.sectors.clear();
					staging.levelData.clear();

					generatorTask = std::async( std::launch::async, std::bind( &SectorIlluminator::getNewTextureData, this ) );

				} else {
					for( const auto& pair : textureData ) {
						if( pair.textureUnit ) {
							Tools::OpenGL::returnTextureUnits( { *pair.textureUnit } );
						}
					}

					textureData = generatorTask.get();

					dirty = false;
				}
			}

			// Otherwise do nothing and stick with textureData
		} else {
			// Future is empty or used - Stage task to run on separate thread
			generatorTask = std::async( std::launch::async, std::bind( &SectorIlluminator::getNewTextureData, this ) );
		}
	}

	std::vector< SectorIlluminator::TextureData > SectorIlluminator::getNewTextureData() const {
		std::unique_lock< std::mutex > lock( mutex );
		std::vector< TextureData > result;

		struct BoundedSector {
			const Sector& sector;
			std::pair< glm::vec3, glm::vec3 > bound;
		};
		std::vector< BoundedSector > constSectors;

		{
			auto boundingBoxes = getSectorBoundingBoxes();
			// Unzip
			int i = 0;
			for( const auto& sector : sectors ) {
				constSectors.emplace_back( BoundedSector{ sector, std::move( boundingBoxes[ i ] ) } );
				i++;
			}
		}

		for( const auto& pair : levelData ) {
			int resolution = std::min( ( int ) std::pow( 10, ConfigManager::getInstance().getIntValue( "sector_resolution" ) ), 100 );
			int height = pair.second.y * resolution;
			int width = pair.second.x * resolution;

			std::unique_ptr< float[] > array = std::make_unique< float[] >( height * width );

			for( int y = 0; y != height; y++ ) {
				for( int x = 0; x != width; x++ ) {
					const glm::vec3 fragment = correctByOrigin( glm::vec3( x / ( float ) resolution, y / ( float ) resolution, pair.first.z ), pair.first );

					// Test fragment using point in polygon against all sectors
					int sectorIndex = 1;
					for( const auto& boundedSector : constSectors ) {
						const Sector& sector = boundedSector.sector;
						std::pair< glm::vec3, glm::vec3 > correctedBound = {
							correctByOrigin( boundedSector.bound.first, pair.first ),
							correctByOrigin( boundedSector.bound.second, pair.first ),
						};
						int fragLevel = int( fragment.z / 4 );

						if(
							fragment.x >= correctedBound.first.x && fragment.y >= correctedBound.first.y &&
							fragment.x <= correctedBound.second.x && fragment.y <= correctedBound.second.y &&
							fragment.z >= fragLevel && fragment.z <= ( fragLevel + 4 )
						) {
							// Generate needle
							std::pair< glm::vec3, glm::vec3 > needle = { fragment, glm::vec3{ getPolygonMaxX( sector, pair.first ) + 1.0f, fragment.y, fragment.z } };

							// Check all sides of this sector against the needle
							unsigned int intersectionCount = 0;
							for( const auto& side : sector.sides ) {
								std::pair< glm::vec3, glm::vec3 > correctedSide = { correctByOrigin( side.first, pair.first ), correctByOrigin( side.second, pair.first ) };
								if( segmentsIntersect( needle, correctedSide ) && fragLevel == int( correctedSide.first.z / 4 ) ) {
									intersectionCount++;
								}
							}

							if( ( intersectionCount % 2 ) != 0 ) {
								// odd means IN!
								array[ ( y * width ) + x ] = sectorIndex;
								break;
							}

							sectorIndex++;
						}
					}
				}
			}

			result.emplace_back( TextureData{
				nullptr, {},
				std::async( std::launch::deferred, [ width, height, array = std::move( array ) ]() {
					return std::make_unique< Texture >( glm::uvec2{ width, height }, array.get() );
				} )
			} );
		}

		return result;
	}

	std::vector< std::pair< glm::vec3, glm::vec3 > > SectorIlluminator::getSectorBoundingBoxes() const {
		std::vector< std::pair< glm::vec3, glm::vec3 > > pairs;

		for( const auto& sector : sectors ) {
			pairs.emplace_back( getBoundingBoxForSector( sector ) );
		}

		return pairs;
	}

	std::pair< glm::vec3, glm::vec3 > SectorIlluminator::getBoundingBoxForSector( const Sector& sector ) const {
		glm::vec2 min{ std::numeric_limits< float >::max(), std::numeric_limits< float >::max() };
		glm::vec2 max{ std::numeric_limits< float >::lowest(), std::numeric_limits< float >::lowest() };

		// zero-length sectors are invalid
		for( const auto& lineSegment : sector.sides ) {
			min.x = std::min( lineSegment.first.x, min.x );
			min.x = std::min( lineSegment.second.x, min.x );

			min.y = std::min( lineSegment.first.y, min.y );
			min.y = std::min( lineSegment.second.y, min.y );

			max.x = std::max( lineSegment.first.x, max.x );
			max.x = std::max( lineSegment.second.x, max.x );

			max.y = std::max( lineSegment.first.y, max.y );
			max.y = std::max( lineSegment.second.y, max.y );
		}

		return { glm::vec3{ min.x, max.y, sector.sides.front().first.z }, glm::vec3{ max.x, min.y, sector.sides.front().first.z } };
	}

	void SectorIlluminator::insert( const Sector& value ) {
		std::unique_lock< std::mutex > lock( mutex, std::defer_lock );
		if( lock.try_lock() ) {
			dirty = true;
			sectors.emplace_back( value );
		} else {
			// Operation in progress - stage it instead
			staging.sectors.emplace_back( value );
		}
	}

	void SectorIlluminator::setLevelData( const glm::vec3& topLeft, const glm::uvec2& dimensions ) {
		std::unique_lock< std::mutex > lock( mutex, std::defer_lock );
		if( lock.try_lock() ) {
			dirty = true;
			levelData.emplace_back( topLeft, dimensions );
		} else {
			// Operation in progress - stage it instead
			staging.levelData.emplace_back( topLeft, dimensions );
		}
	}

}