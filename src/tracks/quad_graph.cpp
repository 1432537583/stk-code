//  $Id$
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2009 Joerg Henrichs
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, B

#include "tracks/quad_graph.hpp"

#include "config/user_config.hpp"
#include "graphics/irr_driver.hpp"
#include "io/file_manager.hpp"
#include "io/xml_node.hpp"
#include "tracks/quad_set.hpp"

const int QuadGraph::UNKNOWN_SECTOR  = -1;

/** Constructor, loads the graph information for a given set of quads
 *  from a graph file.
 *  \param quad_file_name Name of the file of all quads
 *  \param graph_file_name Name of the file describing the actual graph
 */
QuadGraph::QuadGraph(const std::string &quad_file_name, 
                     const std::string graph_file_name)
{
    m_node        = NULL;
    m_mesh        = NULL;
    m_mesh_buffer = NULL;
    m_lap_length  = 0;
    m_all_quads   = new QuadSet(quad_file_name);
    GraphNode::m_all_quads = m_all_quads;
    GraphNode::m_all_nodes = this;
    load(graph_file_name);
}   // QuadGraph

// -----------------------------------------------------------------------------
/** Destructor, removes all nodes of the graph. */
QuadGraph::~QuadGraph()
{
    delete m_all_quads;
    for(unsigned int i=0; i<m_all_nodes.size(); i++) {
        delete m_all_nodes[i];
    }
}   // ~QuadGraph

// -----------------------------------------------------------------------------
/** Loads a quad graph from a file.
 *  \param filename Name of the file to load.
 */
void QuadGraph::load(const std::string &filename)
{
    const XMLNode *xml = file_manager->createXMLTree(filename);

    if(!xml) 
    {
        // No graph file exist, assume a default loop X -> X+1
        // i.e. each quad is part of the graph exactly once.
        // First create an empty graph node for each quad:
        for(unsigned int i=0; i<m_all_quads->getNumberOfQuads(); i++)
            m_all_nodes.push_back(new GraphNode(i));
        // Then set the default loop:
        setDefaultSuccessors();
        return;
    }

    // The graph file exist, so read it in. The graph file must first contain
    // the node definitions, before the edges can be set.
    for(unsigned int node_index=0; node_index<xml->getNumNodes(); node_index++)
    {
        const XMLNode *xml_node = xml->getNode(node_index);
        // First graph node definitions:
        // -----------------------------
        if(xml_node->getName()=="node-list")
        {
            // A list of quads is connected to a list of graph nodes:
            unsigned int from, to;
            xml_node->get("from-quad", &from);
            xml_node->get("to-quad", &to);
            for(unsigned int i=from; i<=to; i++)
            {
                m_all_nodes.push_back(new GraphNode(i));
            }
        }
        else if(xml_node->getName()=="node")
        {
            // A single quad is connected to a single graph node.
            unsigned int id;
            xml_node->get("quad", &id);
            m_all_nodes.push_back(new GraphNode(id));
        }

        // Then the definition of edges between the graph nodes:
        // -----------------------------------------------------
        else if(xml_node->getName()=="edge-loop")
        {
            // A closed loop:
            unsigned int from, to;
            xml_node->get("from", &from);
            xml_node->get("to", &to);
            for(unsigned int i=from; i<=to; i++)
            {
                assert(i!=to ? i+1 : from <m_all_nodes.size());
                m_all_nodes[i]->addSuccessor(i!=to ? i+1 : from);
            }
        }
        else if(xml_node->getName()=="edge")
        {
            // Adds a single edge to the graph:
            unsigned int from, to;
            xml_node->get("from", &from);
            xml_node->get("to", &to);
            assert(to<m_all_nodes.size());
            m_all_nodes[from]->addSuccessor(to);
        }   // edge
        else
        {
            fprintf(stderr, "Incorrect specification in '%s': '%s' ignored\n",
                    filename.c_str(), xml_node->getName().c_str());
            continue;
        }   // incorrect specification
    }
    delete xml;

    for(unsigned int i=0; i<m_all_nodes.size(); i++)
    {
        if(m_all_nodes[i]->getSuccessor(0)==0)
        {
            m_lap_length = m_all_nodes[i]->getDistanceFromStart()
                + m_all_nodes[i]->getDistanceToSuccessor(0);
            break;
        }
    }
    setDefaultSuccessors();
}   // load

// -----------------------------------------------------------------------------
/** This function sets a default successor for all graph nodes that currently
 *  don't have a successor defined. The default successor of node X is X+1.
 */
void QuadGraph::setDefaultSuccessors()
{
    for(unsigned int i=0; i<m_all_nodes.size(); i++) {
        if(m_all_nodes[i]->getNumberOfSuccessors()==0) {
            m_all_nodes[i]->addSuccessor(i+1>=m_all_nodes.size() ? 0 : i+1);
        }   // if size==0
    }   // for i<m_allNodes.size()
}   // setDefaultSuccessors

// -----------------------------------------------------------------------------
/** Creates a mesh for this graph. The mesh is not added to a scene node and 
 *  is stored in m_mesh.
 */
void QuadGraph::createMesh()
{
    // The debug track will not be lighted or culled.
    video::SMaterial m;
    m.BackfaceCulling = false;
    m.Lighting        = false;
    m_mesh            = irr_driver->createQuadMesh(&m);
    m_mesh_buffer     = m_mesh->getMeshBuffer(0);
    assert(m_mesh_buffer->getVertexType()==video::EVT_STANDARD);

    video::SColor     c(255, 255, 0, 0);    
    unsigned int      n     = m_all_quads->getNumberOfQuads();
    // Four vertices for each of the n-1 remaining quads
    video::S3DVertex *new_v = new video::S3DVertex[n*4];
    // Each quad consists of 2 triangles with 3 elements, so 
    // we need 2*3 indices for each quad.
    irr::u16         *ind   = new irr::u16[n*6];

    // Now add all quads
    for(unsigned int i=0; i<n; i++)
    {
        // Swap the colours from red to blue and back
        c.setRed (i%2 ? 255 : 0); 
        c.setBlue(i%2 ? 0 : 255);
        // Transfer the 4 points of the current quad to the list of vertices
        m_all_quads->getQuad(i).getVertices(new_v+4*i, c);

        // Set up the indices for the triangles
        // (note, afaik with opengl we could use quads directly, but the code 
        // would not be portable to directx anymore).
        ind[6*i  ] = 4*i;  // First triangle: vertex 0, 1, 2
        ind[6*i+1] = 4*i+1;
        ind[6*i+2] = 4*i+2;
        ind[6*i+3] = 4*i;  // second triangle: vertex 0, 1, 3
        ind[6*i+4] = 4*i+2;
        ind[6*i+5] = 4*i+3;
    }   // for i=1; i<m_all_quads
    
    m_mesh_buffer->append(new_v, n*4, ind, n*6);
    // Instead of setting the bounding boxes, we could just disable culling,
    // since the debug track should always be drawn.
    //m_node->setAutomaticCulling(scene::EAC_OFF);
    m_mesh_buffer->recalculateBoundingBox();
    m_mesh->setBoundingBox(m_mesh_buffer->getBoundingBox());
}   // createMesh

// -----------------------------------------------------------------------------

/** Creates the debug mesh to display the quad graph on top of the track 
 *  model. */
void QuadGraph::createDebugMesh()
{
    if(m_all_nodes.size()<=0) return;  // no debug output if not graph

    createMesh();

    // Now colour the quads red/blue/red ...
    video::SColor     c(255, 255, 0, 0);    
    video::S3DVertex *v = (video::S3DVertex*)m_mesh_buffer->getVertices();
    for(unsigned int i=0; m_mesh_buffer->getVertexCount(); i++)
    {
        // Swap the colours from red to blue and back
        c.setRed (i%2 ? 255 : 0); 
        c.setBlue(i%2 ? 0 : 255);
        v[i].Color = c;
    }
    m_node           = irr_driver->addMesh(m_mesh);
}   // createDebugMesh

// -----------------------------------------------------------------------------
/** Removes the debug mesh from the scene.
 */
void QuadGraph::cleanupDebugMesh()
{
    irr_driver->removeNode(m_node);
    irr_driver->removeMesh(m_mesh);
}   // cleanupDebugMesh

// -----------------------------------------------------------------------------
/** Returns the list of successors or a node.
 *  \param node_number The number of the node.
 *  \param succ A vector of ints to which the successors are added.
 */
void QuadGraph::getSuccessors(int node_number, std::vector<unsigned int>& succ) const {
    const GraphNode *v=m_all_nodes[node_number];
    for(unsigned int i=0; i<v->getNumberOfSuccessors(); i++) {
        succ.push_back(v->getSuccessor(i));
    }
}   // getSuccessors

//-----------------------------------------------------------------------------
/** This function takes absolute coordinates (coordinates in OpenGL
 *  space) and transforms them into coordinates based on the track. It is
 *  for 2D coordinates, thought it can be used on 3D vectors. The y-axis
 *  of the returned vector is how much of the track the point has gone
 *  through, the x-axis is on which side of the road it is. The Z axis
 *  is not changed.
 *  \param dst Returns the results in the X and Y coordinates.
 *  \param xyz The position of the kart.
 *  \param sector The graph node the position is on.
 */
void QuadGraph::spatialToTrack(Vec3 *dst, const Vec3& xyz, 
                              const int sector) const
{
    if(sector == UNKNOWN_SECTOR )
    {
        std::cerr << "WARNING: UNKNOWN_SECTOR in spatialToTrack().\n";
        return;
    }

    getNode(sector).getDistances(xyz, dst);

}   // spatialToTrack

//-----------------------------------------------------------------------------
/** findRoadSector returns in which sector on the road the position
 *  xyz is. If xyz is not on top of the road, it returns
 *  UNKNOWN_SECTOR.
 *
 *  The 'sector' could be defined as the number of the closest track
 *  segment to XYZ.
 *  \param XYZ Position for which the segment should be determined.
 *  \param sector Contains the previous sector (as a shortcut, since usually
 *         the sector is the same as the last one), and on return the result
 *  \param all_sectors If this is not NULL, it is a list of all sectors to 
 *         test. This is used by the AI to make sure that it ends up on the
 *         selected way in case of a branch, and also to make sure that it
 *         doesn't skip e.g. a loop (see explanation below for details).
 */
void QuadGraph::findRoadSector(const Vec3& xyz, int *sector,
                                std::vector<int> *all_sectors) const
{
    // Most likely the kart will still be on the sector it was before,
    // so this simple case is tested first.
    if(*sector!=UNKNOWN_SECTOR && getQuad(*sector).pointInQuad(xyz) )
    {
        return; 
    }   // if still on same quad

    // Now we search through all graph nodes, starting with
    // the current one
    int indx       = *sector;
    float min_dist = 999999.9f;

    // If a current sector is given, and max_lookahead is specify, only test
    // the next max_lookahead graph nodes instead of testing the whole graph.
    // This is necessary for the AI: if the track contains a loop, e.g.:
    // -A--+---B---+----F--------
    //     E       C
    //     +---D---+
    // and the track is supposed to be driven: ABCDEBF, the AI might find
    // the node on F, and then keep on going straight ahead instead of
    // using the loop at all.
    unsigned int max_count  = (*sector!=UNKNOWN_SECTOR && all_sectors!=NULL) 
                            ? all_sectors->size()
                            : m_all_nodes.size();
    *sector = UNKNOWN_SECTOR;
    for(unsigned int i=0; i<max_count; i++)
    {
        if(all_sectors)
            indx = (*all_sectors)[i];
        else
            indx = indx<(int)m_all_nodes.size()-1 ? indx +1 : 0;
        const Quad &q = getQuad(indx);
        float dist    = xyz.getZ() - q.getMinHeight();
        // While negative distances are unlikely, we allow some small netative
        // numbers in case that the kart is partly in the track.
        if(q.pointInQuad(xyz) && dist < min_dist && dist>-1.0f)
        {
            min_dist = dist;
            *sector  = indx;
        }
    }   // for i<m_all_nodes.size()
    
    return;
}   // findRoadSector

//-----------------------------------------------------------------------------
/** findOutOfRoadSector finds the sector where XYZ is, but as it name
    implies, it is more accurate for the outside of the track than the
    inside, and for STK's needs the accuracy on top of the track is
    unacceptable; but if this was a 2D function, the accuracy for out
    of road sectors would be perfect.

    To find the sector we look for the closest line segment from the
    right and left drivelines, and the number of that segment will be
    the sector.

    The SIDE argument is used to speed up the function only; if we know
    that XYZ is on the left or right side of the track, we know that
    the closest driveline must be the one that matches that condition.
    In reality, the side used in STK is the one from the previous frame,
    but in order to move from one side to another a point would go
    through the middle, that is handled by findRoadSector() which doesn't
    has speed ups based on the side.

    NOTE: This method of finding the sector outside of the road is *not*
    perfect: if two line segments have a similar altitude (but enough to
    let a kart get through) and they are very close on a 2D system,
    if a kart is on the air it could be closer to the top line segment
    even if it is supposed to be on the sector of the lower line segment.
    Probably the best solution would be to construct a quad that reaches
    until the next higher overlapping line segment, and find the closest
    one to XYZ.
 */
int QuadGraph::findOutOfRoadSector(const Vec3& xyz, 
                                   const int curr_sector,
                                   std::vector<int> *all_sectors) const
{
    int count = (all_sectors!=NULL) ? all_sectors->size() : getNumNodes();
    int current_sector = 0;
    if(curr_sector != UNKNOWN_SECTOR && !all_sectors)
    {
        const int LIMIT = 10; //The limit prevents shortcuts
        count           = 2*LIMIT;
        current_sector  = curr_sector - LIMIT;
        if(current_sector<0) current_sector += getNumNodes();
    }

    int   min_sector = UNKNOWN_SECTOR;
    float min_dist_2 = 999999.0f*999999.0f;
    for(int j=0; j<count; j++)
    {
        int next_sector;
        if(all_sectors) 
            next_sector = (*all_sectors)[j];
        else
            next_sector  = current_sector+1 == (int)getNumNodes() ? 0 : current_sector+1;

        float dist_2 = xyz.distance2(getQuad(next_sector).getCenter()-xyz);
        if(dist_2<min_dist_2)
        {
            min_dist_2 = dist_2;
            min_sector = next_sector;
        }
        current_sector = next_sector;
    }   // for j

    if(min_sector==UNKNOWN_SECTOR )
    {
        printf("unknown sector found.\n");
    }
    return min_sector;
}   // findOutOfRoadSector

//-----------------------------------------------------------------------------
/** Draws the mini map on the screen.
 *  \param where the top left and lower right corner for the mini map.
 */
#ifdef IRR_SVN
video::ITexture *QuadGraph::makeMiniMap(const core::dimension2du &dimension,
                                        const std::string &name,
                                        const video::SColor &fill_color)
#else
video::ITexture *QuadGraph::makeMiniMap(const core::dimension2di &dimension,
                                        const std::string &name,
                                        const video::SColor &fill_color)
#endif
{
    irr_driver->beginRenderToTexture(dimension, name);
    createMesh();   
    video::S3DVertex *v = (video::S3DVertex*)m_mesh_buffer->getVertices();
    for(unsigned int i=0; i<m_mesh_buffer->getVertexCount(); i++)
    {
        v[i].Color = fill_color;
    }

    m_node = irr_driver->addMesh(m_mesh);   // add Debug Mesh
    m_node->setMaterialFlag(video::EMF_LIGHTING, false);

    // Add the camera:
    // ---------------
    scene::ICameraSceneNode *camera = irr_driver->addCamera();
    Vec3 bb_min, bb_max;
    m_all_quads->getBoundingBox(&bb_min, &bb_max);
    Vec3 center = (bb_max+bb_min)*0.5f;
    core::matrix4 projection;
    projection.buildProjectionMatrixOrthoLH(bb_max.getX()-bb_min.getX(), 
                                            bb_max.getY()-bb_min.getY(),
                                            -1, bb_max.getZ()-bb_min.getZ()+1);
    camera->setProjectionMatrix(projection, true);
    camera->setPosition(core::vector3df(center.getX(), bb_max.getZ(), center.getY()));
    camera->setUpVector(core::vector3df(0,0,1));
    camera->setTarget(core::vector3df(center.getX(),0,center.getY()));
    video::ITexture *texture=irr_driver->endRenderToTexture();
    cleanupDebugMesh();
    irr_driver->removeCamera(camera);
    m_min_coord = bb_min;
    m_scaling.setX(dimension.Width/(bb_max.getX()-bb_min.getX()));
    m_scaling.setY(dimension.Width/(bb_max.getY()-bb_min.getY()));
    return texture;

}   // drawMiniMap
//-----------------------------------------------------------------------------
    /** Returns the 2d coordinates of a point when drawn on the mini map 
     *  texture.
     *  \param xyz Coordinates of the point to map.
     *  \param draw_at The coordinates in pixel on the mini map of the point,
     *         only the first two coordinates will be used.
     */
void QuadGraph::mapPoint2MiniMap(const Vec3 &xyz,Vec3 *draw_at) const
{
    draw_at->setX((xyz.getX()-m_min_coord.getX())*m_scaling.getX());
    draw_at->setY((xyz.getY()-m_min_coord.getY())*m_scaling.getY());

}   // mapPoint