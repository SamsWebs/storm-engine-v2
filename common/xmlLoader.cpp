#include "xmlLoader.h"

namespace storm {

void LoadTexturesFromXml(const std::string &filePath,
                         const std::string &stateId,
                         const std::string &basePath,
                         SDL_Renderer      *renderer,
                         AssetStore        *assetStore,
                         Logger            *logger) {
    XmlLoader xml(filePath);
    if (!xml.IsValid()) {
        logger->Err("LoadTexturesFromXml: failed to load " + filePath);
        return;
    }
    for (const auto &tex : xml.GetTextures(stateId)) {
        std::string path = basePath + tex.filename;
        assetStore->AddTexture(renderer, tex.id, path.c_str());
        logger->Log("Loaded texture '" + tex.id + "' from " + path);
    }
}

XmlLoader::XmlLoader(const std::string &filePath) {
    valid_ = (doc_.LoadFile(filePath.c_str()) == tinyxml2::XML_SUCCESS);
}

bool XmlLoader::IsValid() const {
    return valid_;
}

std::vector<XmlTextureDef> XmlLoader::GetTextures(const std::string &stateId) const {
    std::vector<XmlTextureDef> results;
    auto *textures = FindElement(stateId, "TEXTURES");
    if (!textures) return results;

    for (auto *t = textures->FirstChildElement("texture"); t;
         t = t->NextSiblingElement("texture")) {
        const char *id = t->Attribute("ID");
        const char *fn = t->Attribute("filename");
        if (id && fn)
            results.push_back({id, fn});
    }
    return results;
}

std::vector<XmlObjectDef> XmlLoader::GetObjects(const std::string &stateId) const {
    std::vector<XmlObjectDef> results;
    auto *objects = FindElement(stateId, "OBJECTS");
    if (!objects) return results;

    for (auto *obj = objects->FirstChildElement("object"); obj;
         obj = obj->NextSiblingElement("object")) {
        XmlObjectDef def;

        if (auto *v = obj->Attribute("type"))      def.type      = v;
        if (auto *v = obj->Attribute("textureID")) def.textureId = v;

        obj->QueryFloatAttribute("x",         &def.x);
        obj->QueryFloatAttribute("y",         &def.y);
        obj->QueryIntAttribute  ("width",     &def.width);
        obj->QueryIntAttribute  ("height",    &def.height);
        obj->QueryIntAttribute  ("numFrames", &def.numFrames);
        obj->QueryIntAttribute  ("zIndex",    &def.zIndex);

        // Store any remaining attributes as raw strings
        for (auto *attr = obj->FirstAttribute(); attr; attr = attr->Next()) {
            std::string name = attr->Name();
            if (name != "type"      && name != "textureID" &&
                name != "x"         && name != "y"         &&
                name != "width"     && name != "height"    &&
                name != "numFrames" && name != "zIndex")
                def.attributes[name] = attr->Value();
        }

        results.push_back(def);
    }
    return results;
}

tinyxml2::XMLElement *XmlLoader::FindElement(const std::string &stateId,
                                              const std::string &child) const {
    const auto *root = doc_.FirstChildElement("States");
    if (!root) return nullptr;
    const auto *state = root->FirstChildElement(stateId.c_str());
    if (!state) return nullptr;
    return const_cast<tinyxml2::XMLElement *>(
        state->FirstChildElement(child.c_str()));
}

} // namespace storm
