#include <igloo/igloo_alt.h>

#include "../common/xmlLoader.h"

using namespace igloo;
using namespace storm;

static const std::string filePath = "./specs/assets/xml/states.xml";

Describe(XmlLoaderSpec) {

  Describe(IsValidSpec) {
    It(should_be_valid_for_an_existing_well_formed_file) {
      XmlLoader xml(filePath);
      Assert::That(xml.IsValid(), Equals(true));
    };

    It(should_be_invalid_for_a_missing_file) {
      XmlLoader xml("./specs/assets/xml/does_not_exist.xml");
      Assert::That(xml.IsValid(), Equals(false));
    };
  };

  Describe(GetTexturesSpec) {
    It(should_return_all_textures_for_a_state) {
      XmlLoader xml(filePath);
      auto textures = xml.GetTextures("PLAY");
      Assert::That(textures.size(), Equals(2u));
    };

    It(should_read_texture_id_and_filename) {
      XmlLoader xml(filePath);
      auto textures = xml.GetTextures("PLAY");
      Assert::That(textures[0].id, Equals("player"));
      Assert::That(textures[0].filename, Equals("player.png"));
      Assert::That(textures[1].id, Equals("tiles"));
      Assert::That(textures[1].filename, Equals("tiles.png"));
    };

    It(should_return_empty_for_a_state_without_a_textures_section) {
      XmlLoader xml(filePath);
      auto textures = xml.GetTextures("EMPTY");
      Assert::That(textures.size(), Equals(0u));
    };

    It(should_return_empty_for_an_unknown_state) {
      XmlLoader xml(filePath);
      auto textures = xml.GetTextures("NONEXISTENT");
      Assert::That(textures.size(), Equals(0u));
    };
  };

  Describe(GetObjectsSpec) {
    It(should_return_all_objects_for_a_state) {
      XmlLoader xml(filePath);
      auto objects = xml.GetObjects("PLAY");
      Assert::That(objects.size(), Equals(2u));
    };

    It(should_read_type_and_texture_id) {
      XmlLoader xml(filePath);
      auto objects = xml.GetObjects("PLAY");
      Assert::That(objects[0].type, Equals("Player"));
      Assert::That(objects[0].textureId, Equals("player"));
    };

    It(should_read_integer_and_float_attributes) {
      XmlLoader xml(filePath);
      auto objects = xml.GetObjects("PLAY");
      Assert::That(objects[0].x, Equals(100.f));
      Assert::That(objects[0].y, Equals(200.f));
      Assert::That(objects[0].width, Equals(64));
      Assert::That(objects[0].height, Equals(48));
      Assert::That(objects[0].numFrames, Equals(4));
      Assert::That(objects[0].zIndex, Equals(3));
    };

    It(should_parse_fractional_float_positions) {
      XmlLoader xml(filePath);
      auto objects = xml.GetObjects("PLAY");
      Assert::That(objects[1].x, Equals(10.5f));
      Assert::That(objects[1].y, Equals(20.25f));
    };

    It(should_fall_back_to_defaults_for_missing_attributes) {
      // The Enemy object omits width/height/numFrames/zIndex.
      XmlLoader xml(filePath);
      auto objects = xml.GetObjects("PLAY");
      Assert::That(objects[1].width, Equals(32));
      Assert::That(objects[1].height, Equals(32));
      Assert::That(objects[1].numFrames, Equals(1));
      Assert::That(objects[1].zIndex, Equals(0));
    };

    It(should_capture_unknown_attributes_in_the_attributes_map) {
      XmlLoader xml(filePath);
      auto objects = xml.GetObjects("PLAY");
      Assert::That(objects[1].attributes.at("health"), Equals("50"));
      Assert::That(objects[1].attributes.at("speed"), Equals("120"));
    };

    It(should_not_capture_known_attributes_in_the_attributes_map) {
      XmlLoader xml(filePath);
      auto objects = xml.GetObjects("PLAY");
      Assert::That(objects[0].attributes.find("x") == objects[0].attributes.end(),
                   Equals(true));
      Assert::That(objects[0].attributes.find("type") == objects[0].attributes.end(),
                   Equals(true));
    };

    It(should_return_empty_for_a_state_without_an_objects_section) {
      XmlLoader xml(filePath);
      auto objects = xml.GetObjects("MENU");
      Assert::That(objects.size(), Equals(0u));
    };

    It(should_return_empty_for_an_unknown_state) {
      XmlLoader xml(filePath);
      auto objects = xml.GetObjects("NONEXISTENT");
      Assert::That(objects.size(), Equals(0u));
    };
  };
};
