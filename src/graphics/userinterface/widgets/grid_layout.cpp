#include "graphics/userinterface/widgets/grid_layout.hpp"
#include "graphics/userinterface/utility.hpp"

namespace BlueBear::Graphics::UserInterface::Widgets {

	GridLayout::GridLayout( const std::string& id, const std::vector< std::string >& classes )
		: Element::Element( "GridLayout", id, classes ) {}

	std::shared_ptr< GridLayout > GridLayout::create( const std::string& id, const std::vector< std::string >& classes ) {
		std::shared_ptr< GridLayout > gridLayout( new GridLayout( id, classes ) );

		return gridLayout;
	}

	glm::ivec2 GridLayout::getGridDimensions() const {
		LayoutProportions columns = localStyle.get< LayoutProportions >( "grid-columns" );

		// Number of children / columns = rows
		// Add an extra row for any nonzero remainder
		int calculatedRows = children.size() / columns.size();
		if( children.size() % columns.size() ) {
			calculatedRows++;
		}

		return { columns.size(), calculatedRows };
	}

	std::vector< int > GridLayout::getColumnSizes() const {
		const auto& columns = localStyle.get< LayoutProportions >( "grid-columns" );
		std::vector< int > result;

		int total = 0;
		for( const int column : columns ) {
			total += column;
		}

		for( const int column : columns ) {
			float percent = ( float ) column / ( float ) total;
			result.emplace_back( percent * allocation[ 2 ] );
		}

		return result;
	}

	std::vector< int > GridLayout::getRowSizes( int numColumns ) const {
		int total = children.size() / numColumns;
		if( children.size() % numColumns ) {
			total++;
		}

		std::vector< int > result( total, ( 1.0f / ( float ) total ) * allocation[ 3 ] );
		const auto& rows = localStyle.get< LayoutProportions >( "grid-rows" );
		for( int i = 0; i < rows.size() && i < result.size(); i++ ) {
			float percent = ( float ) rows[ i ] / ( float ) total;
			result[ i ] = percent * allocation[ 3 ];
		}

		return result;
	}

	// Heavy TODO
	void GridLayout::positionAndSizeChildren() {
		auto columnSizes = getColumnSizes();
		auto rowSizes = getRowSizes( columnSizes.size() );

		glm::ivec2 gridDimensions = { columnSizes.size(), rowSizes.size() };

		int i = 0;
		for( auto& child : children ) {
			glm::ivec2 gridCell{ i % gridDimensions.x, i / gridDimensions.x };

			glm::ivec2 childPosition{
				[ & ] {
					int total = 0;

					for( int x = 0; x != gridCell.x; x++ ) {
						total += columnSizes[ x ];
					}

					return total;
				}(),
				[ & ] {
					int total = 0;

					for( int y = 0; y != gridCell.y; y++ ) {
						total += rowSizes[ y ];
					}

					return total;
				}()
			};

			child->setAllocation( {
				childPosition.x,
				childPosition.y,
				columnSizes[ gridCell.x ],
				rowSizes[ gridCell.y ]
			}, false );

			i++;
		}
	}

	/**
	 * No drawable will be rendered.
	 */
	bool GridLayout::drawableDirty() {
		return false;
	}

	/**
	 * No drawable will be rendered.
	 */
	void GridLayout::generateDrawable() {}

	void GridLayout::calculate() {
		glm::ivec2 dimensions = getGridDimensions();
		glm::uvec2 maxRequired{ 0, 0 };

		for( auto& child : children ) {
			child->calculate();
			glm::uvec2 requisition = Utility::getFinalRequisition( child );

			maxRequired.x = std::max( maxRequired.x, requisition.x );
			maxRequired.y = std::max( maxRequired.y, requisition.y );
		}

		requisition = {
			dimensions.x * maxRequired.x,
			dimensions.y * maxRequired.y
		};
	}

}