//
//  glmLabelManager.cpp
//  Labeling
//
//  Created by Patricio Gonzalez Vivo on 9/4/14.
//
//

#include "glmLabelManager.h"
#include <algorithm>

glmLabelManager::glmLabelManager(): minDistance(200), bLines(true), bPoints(true), bDebugLines(false), bUpdateSegments(false), bDebugPoints(false), bDebugField(false), bDebugGrid(false), m_bFontChanged(true), m_bChange(true) {
}

glmLabelManager::~glmLabelManager(){
    
}

void glmLabelManager::setFont(glmFont *_font){
    m_font = glmFontRef(_font);
    m_bFontChanged = true;
}

void glmLabelManager::setFont(glmFontRef &_font){
    m_font = _font;
    m_bFontChanged = true;
}

glmFontRef& glmLabelManager::getFont(){
    return m_font;
}

void glmLabelManager::addLineLabel( glmFeatureLabelLineRef &_lineLabel ){
    if(m_font != NULL){
        _lineLabel->setFont(m_font);
    }
    _lineLabel->setCameraPos(&m_cameraPos);
    _lineLabel->pointLabels = &pointLabels;
    
    lineLabels.push_back(_lineLabel);
    m_bChange = true;
    
    //  TODO: Check duplicates street names
}

void glmLabelManager::addPointLabel( glmFeatureLabelPointRef &_pointLabel ){
    if(m_font != NULL){
        _pointLabel->setFont(m_font);
    }
    _pointLabel->setCameraPos(&m_cameraPos);
    
    bool isPrev = false;
    for (int i = 0; i < pointLabels.size(); i++) {
        
        if(pointLabels[i]->idString == _pointLabel->idString ){
           
            mergePointLabels(pointLabels[i],_pointLabel);
            
            isPrev = true;
            break;
        }
    }
    
    if(!isPrev){
        pointLabels.push_back(_pointLabel);
    }
    
    m_bChange = true;
}

void glmLabelManager::mergePointLabels( glmFeatureLabelPointRef &_father, glmFeatureLabelPointRef &_child){
    
    //  Move shapes from _child to father
    //
    for (int i = _child->shapes.size()-1; i >= 0; i--) {
        _father->shapes.push_back(_child->shapes[i]);
        _child->shapes.erase(_child->shapes.begin()+i);
    }
    
    //  Re center father
    //
    int maxHeight = 0;
    glm::vec3 center;
    for (auto &it: _father->shapes) {
        if (it[0].z > maxHeight) {
            maxHeight = it[0].z;
        }
        center += it.getCentroid();
    }
    center = center / (float)_father->shapes.size();
    center.z = maxHeight;
    _father->setPosition(center);
    
}

bool glmLabelManager::deleteLabel(const std::string &_idString){
    for (int i = lineLabels.size()-1 ; i >= 0 ; i--) {
        if (lineLabels[i]->idString == _idString) {
            lineLabels.erase(lineLabels.begin()+i);
            break;
        }
    }
    
    for (int i = pointLabels.size()-1 ; i >= 0 ; i--) {
        if (pointLabels[i]->idString == _idString) {
            pointLabels.erase(pointLabels.begin()+i);
            break;
        }
    }
    
    m_bChange = true;
}

void glmLabelManager::updateFont(){
    if(m_bFontChanged){
        for (auto &it : pointLabels) {
            it->setFont(m_font);
        }
        
        for (auto &it : lineLabels) {
            it->setFont(m_font);
        }
        
        m_bFontChanged = false;
    }
}

bool depthSort(const glmFeatureLabelPointRef &_A, const glmFeatureLabelPointRef &_B){
    return _A->getAnchorPoint().z < _B->getAnchorPoint().z;
}

void glmLabelManager::updateCameraPosition( const glm::vec3 &_pos ){
    if(m_cameraPos != _pos){
        m_cameraPos = _pos;
        m_bProjectionChanged = true;
    }
}

void glmLabelManager::updateProjection(){
    
    if(m_bFontChanged){
        updateFont();
    }
    
    //  Viewport change?
    //
    glm::ivec4 viewport;
    glGetIntegerv(GL_VIEWPORT, &viewport[0]);
    if(m_viewport != viewport){
        m_viewport = viewport;
        
        m_field.set(m_viewport[2], m_viewport[3], 100);
        m_field.addRepelForce(glm::vec3(viewport[2]*0.5,viewport[3]*0.5,0.0), viewport[2], 10.0);
        m_field.addRepelBorders(20);
        
        m_bProjectionChanged = true;
    }
    
    //  Projection Change? New Labels?
    //
    if(m_bProjectionChanged || m_bChange ){
        
        //  Get transformation matrixes
        //
        glm::mat4x4 mvmatrix, projmatrix;
        glGetFloatv(GL_MODELVIEW_MATRIX, &mvmatrix[0][0]);
        glGetFloatv(GL_PROJECTION_MATRIX, &projmatrix[0][0]);

        //  UPDATE POINTS
        //
        if(bPoints){
            
            //  Update Projection
            //
            for (auto &it : pointLabels) {

                it->m_anchorLines.clear();
                if(bUpdateSegments){
                    glmPolyline allPoints;
                    for (auto &shape: it->shapes) {
                        glmAnchorLine line;
                        for (int i = 0; i < shape.size(); i++) {
                            glm::vec3 v = glm::project(shape[i], mvmatrix, projmatrix, viewport);
                            if( v.z >= 0.0 && v.z <= 1.0){
                                line.add(v);
                            }
                        }
                        it->m_anchorLines.push_back(line);
                    }
                }
                
                it->m_anchorPoint = glm::project(it->m_centroid+it->m_offset, mvmatrix, projmatrix, viewport);
                it->m_projectedCentroid = glm::project(it->m_centroid, mvmatrix, projmatrix, viewport);
                
                it->update();
            }
            
            //  Depth Sort
            //
            std::sort(pointLabels.begin(),pointLabels.end(), depthSort);
            
            //  Check for oclussions
            //
            for (int i = 0; i < pointLabels.size(); i++) {
                if(pointLabels[i]->bVisible){
                    for (int j = i-1; j >= 0 ; j--) {
                        if (pointLabels[i]->isOver( pointLabels[j].get(), minDistance ) ){
                            pointLabels[i]->bVisible = false;
                            break;
                        }
                    }
                }
            }
        } else {
            for (auto &it : pointLabels) {
                it->bVisible = false;
            }
        }
        
        //  LINES
        //
        if(bLines){
            for (auto &it : lineLabels) {

                //  Match number of m_anchorLines
                //
                if (it->m_anchorLines.size() != it->shapes.size() ){
                    it->m_anchorLines.resize(it->shapes.size());
                }
                
                //  Project the road into 2D screen position
                //
                it->bVisible = false;
                for (int i = 0; i < it->m_anchorLines.size(); i++){
                    
                    it->m_anchorLines[i].project(it->shapes[i], mvmatrix, projmatrix, viewport);
                    
                    //  With only one line visible the line is tag as visible
                    //
                    if (it->m_anchorLines[i].m_bVisible){
                        it->bVisible = true;
                    }
                }
                
                if (it->bVisible) {
                    
                    //  Seed the positions for line labels
                    //
                    for (auto &line: it->m_anchorLines) {
                        
                        line.m_nSegmentLabels = it->seedAnchorOnSegmentsEvery(line, 50);
                        
//                        if(line.m_nSegmentLabels == 0){
//                            it->seedAnchorsEvery(line, minDistance);
//                        }
                        
                    }
                }
                
            }
        } else {
            for (auto &it : lineLabels) {
                it->bVisible = false;
            }
        }
        
        m_bProjectionChanged = false;
        m_bChange = false;
    }
}

void glmLabelManager::draw2D(){
    
    glColor4f(1.,1.,1.,1.);
    
    for (auto &it : lineLabels) {
        if(bDebugLines){
            it->drawDebug();
        }
        it->draw2D();
    }
    
    for (auto &it : pointLabels) {
        if(bDebugPoints){
            it->drawDebug();
        }
        it->draw2D();
    }
    
    if(bDebugGrid){
        glLineWidth(0.01);
        glColor4f(m_font->colorFront.r,m_font->colorFront.g,m_font->colorFront.b,0.3);
        m_field.drawGrid();
    }
    
    if(bDebugField){
        glLineWidth(0.01);
        glColor4f(m_font->colorFront.r,m_font->colorFront.g,m_font->colorFront.b,0.8);
        m_field.drawForces();
    }
    
    glColor4f(1.,1.,1.,1.);
}

void glmLabelManager::draw3D(){
    if(bPoints){
        glColor4f(1.,1.,1.,1.);
        for (auto &it : pointLabels) {
            it->draw3D();
        }
    }
}