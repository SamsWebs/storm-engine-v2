#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <tinyxml2.h>
#include <SDL2/SDL.h>

#include "assetStore.h"
#include "logger.h"

namespace storm {

// ─────────────────────────────────────────────────────────────────────────────
// Data structures returned by XmlLoader — no ECS, no AssetStore, no SDL.
// The caller decides what to do with the results.
// ─────────────────────────────────────────────────────────────────────────────

struct XmlTextureDef {
    std::string id;        // value of ID attribute
    std::string filename;  // value of filename attribute
};

struct XmlObjectDef {
    std::string type;
    std::string textureId;
    float       x         = 0.f;
    float       y         = 0.f;
    int         width     = 32;
    int         height    = 32;
    int         numFrames = 1;
    int         zIndex    = 0;

    // Any extra attributes not listed above
    std::unordered_map<std::string, std::string> attributes;
};

// ─────────────────────────────────────────────────────────────────────────────
// LoadTexturesFromXml
//
// Convenience helper: reads all <texture> entries for the given stateId and
// loads them into the AssetStore.  Texture file paths are resolved as
// basePath + filename.
// ─────────────────────────────────────────────────────────────────────────────
void LoadTexturesFromXml(const std::string &filePath,
                         const std::string &stateId,
                         const std::string &basePath,
                         SDL_Renderer      *renderer,
                         AssetStore        *assetStore,
                         Logger            *logger);

// ─────────────────────────────────────────────────────────────────────────────
// XmlLoader
//
// Parses an XML file structured as:
//
//   <States>
//     <SOMESTATE>
//       <TEXTURES>
//         <texture filename="foo.png" ID="foo"/>
//       </TEXTURES>
//       <OBJECTS>
//         <object type="..." x="0" y="0" width="32" height="32"
//                 textureID="foo" numFrames="1" zIndex="0"/>
//       </OBJECTS>
//     </SOMESTATE>
//   </States>
// ─────────────────────────────────────────────────────────────────────────────
class XmlLoader {
public:
    explicit XmlLoader(const std::string &filePath);

    bool IsValid() const;

    std::vector<XmlTextureDef> GetTextures(const std::string &stateId) const;
    std::vector<XmlObjectDef>  GetObjects (const std::string &stateId) const;

private:
    tinyxml2::XMLDocument doc_;
    bool                  valid_ = false;

    tinyxml2::XMLElement *FindElement(const std::string &stateId,
                                     const std::string &child) const;
};

} // namespace storm
