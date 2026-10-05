#include "FileLoader.h"

namespace fs = std::filesystem;

Registry AssetManager::registry;

FileLoader::FileLoader() {}

FileLoader::~FileLoader() {}

void FileLoader::LoadProject(sol::state &lua, const std::string &filename,
                             const AssetManager_Ptr &assetManager,
                             Renderer &renderer,
                             std::vector<std::string> &assetIds,
                             std::vector<std::string> &assetFilepaths,
                             std::shared_ptr<Canvas> &canvas, int &tileSize) {
  /*
          Things that need to be set by load project:
                  - TileSize
                  - Canvas Width
                  - Canvas Height
                  - tilesets --> could be more than one -->ID and file Location
                          - These need to be loaded into a vector<std::string>
                  - tilemaps --> File Location

  */

  fs::path filepath(filename);

  if (filepath.extension() != ".lua") {
    logger.Err("FILELOADER__LINE__44: Project File must be a [.lua] file!");
    // TODO: ADD ERROR MSG TO IMGUI
    return;
  }

  sol::load_result script = lua.load_file(filename);

  // Check to see if the script is a valid Lua file
  if (!script.valid()) {
    sol::error err = script;
    std::string errorMsg = err.what();
    logger.Err("FILELOADER__LINE__55: Error loading the Lua Script -- " +
               errorMsg);
    // TODO: ADD ERROR MSG TO IMGUI
    return;
  }

  // Execute the script
  lua.script_file(filename);

  // Opening a project replaces the one on screen, so the old world goes
  // first. Without this the new tiles are appended to it: both render on top
  // of each other, and the next Save walks the "tiles" group and writes the
  // previous project's tiles into whichever file is open now. CreateNewCanvas
  // is the only other path that clears, and Open does not go through it.
  // Done after the script parses and runs, so a bad project file leaves the
  // current one alone.
  assetIds.clear();
  assetFilepaths.clear();
  for (const char *group : {"tiles", "colliders"}) {
    if (!Registry::Instance().DoesGroupExist(group))
      continue;
    for (Entity entity : Registry::Instance().GetEntitiesByGroup(group))
      entity.Kill();
  }
  // Entity destruction is deferred; flush it before the load creates new ones.
  Registry::Instance().Update();

  sol::table project = lua["project"];
  int assetNum = 0;
  int mapNum = 0;

  std::string mapFile = "";

  while (true) {
    sol::optional<sol::table> hasAssets = project["assets"][assetNum];
    if (hasAssets == sol::nullopt) {
      logger.Log("FILELOADER__LINE_75: Finished loading assets");
      break;
    }

    sol::table assets = project["assets"][assetNum];
    std::string assetId = assets["asset_id"];
    std::string file_path = assets["file_path"];

    assetIds.push_back(assetId);
    assetFilepaths.push_back(file_path);

    assetManager->AddTexture(renderer, std::move(assetId),
                             std::move(file_path));

    assetNum++;
  }

  while (true) {
    sol::optional<sol::table> hasMaps = project["maps"][mapNum];
    if (hasMaps == sol::nullopt) {
      logger.Log("FILELOADER: Finished loading maps");
      break;
    }

    sol::table maps = project["maps"][mapNum];
    mapFile = maps["file_path"];

    mapNum++;
  }

  sol::optional<sol::table> luaCanvas = project["canvas"];
  if (luaCanvas != sol::nullopt) {
    int canvasWidth = project["canvas"]["canvas_width"];
    int canvasHeight = project["canvas"]["canvas_height"];
    tileSize = project["canvas"]["tile_size"];

    canvas->SetWidth(std::move(canvasWidth));
    canvas->SetHeight(std::move(canvasHeight));
  }

  LoadMap(assetManager, mapFile);
}

void FileLoader::LoadMap(const AssetManager_Ptr &assetManager,
                         const std::string &filename) {
  // Open and read the tilemap
  std::fstream mapFile;
  mapFile.open(filename);

  if (!mapFile.is_open()) {
    logger.Err("FILELOADER__LINE__37: Unable to open[{0}] for loading " +
               filename);
    return;
  }

  // THE VERSION HEADER, read from THIS stream, exactly as the engine's loader
  // does. It did not used to: LoadMap went straight into reading records, so
  // on any versioned map it read "storm-map" as the first record's group name
  // and loaded ZERO tiles out of a file the engine itself had written. The
  // editor could not open a 2.7.0 map at all.
  const TileMapVersion version = ReadTileMapVersion(mapFile);
  const std::string refusal = TileMapVersionRefusal(version, filename);
  if (!refusal.empty()) {
    logger.Err(refusal);
    return;
  }

  // The record parsing is NOT here either -- it is TileRecordReader, shared
  // with the engine's TileMapLoader. That sharing is the whole point, and it
  // fixes a live bug: this function used to do `mapFile >> animated;` with no
  // pushback, so a record that omitted its animation flag consumed the NEXT
  // record's group token, set failbit, and ended the map right there. The
  // engine's copy had already been fixed for exactly that and this one had not,
  // which is what "two hand-written parsers drift" looks like in practice.
  TileRecordReader reader(mapFile);

  TileRecord record;
  while (reader.Read(&record)) {
    Entity tile = Registry::Instance().CreateEntity();
    tile.Group(record.group);
    tile.AddComponent<SpriteComponent>(record.assetId, record.tileWidth,
                                       record.tileHeight, record.zIndex, false,
                                       record.srcRectX, record.srcRectY);
    tile.AddComponent<TransformComponent>(
        glm::vec2(record.worldX, record.worldY),
        glm::vec2(record.scaleX, record.scaleY), 0.0);

    if (record.collider)
      tile.AddComponent<BoxColliderComponent>(
          record.colliderWidth, record.colliderHeight,
          glm::vec2(record.colliderOffsetX, record.colliderOffsetY));
    if (record.animated)
      tile.AddComponent<AnimationComponent>(record.numFrames, record.frameSpeed,
                                            record.vertical, record.looped,
                                            record.frameOffset);
  }

  // A failed Read is either "the map ended" or "the map is damaged", and the
  // loop above cannot tell them apart. It used to `break` on both, with nothing
  // said -- a truncated map loaded as a short map and looked like content.
  if (reader.Truncated()) {
    logger.Err("FileLoader: '" + filename + "': truncated or malformed " +
               reader.TruncatedWhat() + "; the map above is incomplete");
  }

  // Close the file
  mapFile.close();
}

void FileLoader::SaveMap(std::filesystem::path filename) {
  // Check to see if there are any tile entities to save to a file
  if (!Registry::Instance().DoesGroupExist("tiles")) {
    logger.Err(
        "FILELOADER__LINE__85: Trying to save entities that do not exist!");
    return;
  }

  std::ofstream mapFile;

  mapFile.open(filename);

  if (!mapFile.is_open()) {
    // logger.Err("FILELOADER__LINE__97: Unable to open[{0}] for saving " +
    //            filename);
    return;
  }

  // The format version, written by the SAME function the engine reads it with.
  //
  // This used to be the second parser of this format in a second binary, which
  // is the failure mode the roadmap named: the editor is a separate artifact
  // that has to be rebuilt and repackaged in the same release as the engine, so
  // a change to the header had to be made twice and the second time was the one
  // that got forgotten. Now the number exists once, in
  // <stormengine2/tilemapFormat.h>, and this file cannot stamp a version the
  // engine would refuse.
  mapFile << TileMapVersionLine();

  auto tiles = Registry::Instance().GetEntitiesByGroup("tiles");

  // The 22 fields are written by WriteTileRecord, the same function the
  // engine's TileRecordReader reads. Previously this loop spelled the order out
  // by hand, which meant the editor and the engine each carried their own copy
  // of the field order and a change to it had to be made twice.
  for (const auto &tile : tiles) {
    const auto &sprite = tile.GetComponent<SpriteComponent>();
    const auto &transform = tile.GetComponent<TransformComponent>();

    TileRecord record;
    record.group = "tiles";
    record.assetId = sprite.assetId;
    record.tileWidth = sprite.width;
    record.tileHeight = sprite.height;
    record.srcRectX = sprite.srcRect.x;
    record.srcRectY = sprite.srcRect.y;
    record.zIndex = sprite.zIndex;
    record.worldX = transform.position.x;
    record.worldY = transform.position.y;
    record.scaleX = transform.scale.x;
    record.scaleY = transform.scale.y;

    if (tile.HasComponent<BoxColliderComponent>()) {
      const auto &box = tile.GetComponent<BoxColliderComponent>();
      record.collider = true;
      record.colliderWidth = box.width;
      record.colliderHeight = box.height;
      record.colliderOffsetX = box.offset.x;
      record.colliderOffsetY = box.offset.y;
    }

    if (tile.HasComponent<AnimationComponent>()) {
      const auto &animation = tile.GetComponent<AnimationComponent>();
      record.animated = true;
      record.numFrames = animation.numFrames;
      record.frameSpeed = animation.frameSpeedRate;
      record.vertical = animation.vertical;
      record.looped = animation.isLooped;
      record.frameOffset = animation.frameOffset;
    }

    WriteTileRecord(mapFile, record);
  }
  // Close the file
  mapFile.close();
}

void FileLoader::SaveColliders(std::filesystem::path filename) {
  // Check to see if there are any tile entities to save to a file
  if (!Registry::Instance().DoesGroupExist("colliders")) {
    logger.Err(
        "FILELOADER__LINE__220: Trying to save Colliders that do not exist!");
    return;
  }

  std::ofstream mapFile;

  mapFile.open(filename);

  if (!mapFile.is_open()) {
    // logger.Err("FILELOADER__LINE__230: Unable to open[{0}] for saving " +
    //            filename);
    return;
  }

  // Same reason as SaveMap: the collider map is read back through the same
  // loader, so it carries the same header from the same function. A map whose
  // two files disagreed about their version would be the exact confusion the
  // header was added to remove.
  mapFile << TileMapVersionLine();

  auto colliders = Registry::Instance().GetEntitiesByGroup("colliders");

  for (const auto &collider : colliders) {
    std::string group = "collider";
    const auto &boxCollider = collider.GetComponent<BoxColliderComponent>();
    const auto &transform = collider.GetComponent<TransformComponent>();

    // Save to the map file
    mapFile << group << " " << transform.position.x << " "
            << transform.position.y << " " << transform.scale.x << " "
            << transform.scale.y << " " << boxCollider.width << " "
            << boxCollider.height << " " << boxCollider.offset.x << " "
            << boxCollider.offset.y << " " << std::endl;
  }
  // Close the file
  mapFile.close();
}

void FileLoader::SaveProject(const std::string &filename,
                             std::vector<std::string> &assetIds,
                             std::vector<std::string> &assetFilepaths,
                             const int &canvasWidth, const int &canvasHeight,
                             const int &tileSize) {

  std::fstream projFile;
  projFile.open(filename, std::ios::out | std::ios::trunc);

  if (!projFile.is_open()) {
    logger.Err("FILELOADER__LINE__223: Unable to open[{0}] for saving " +
               filename);
    return;
  }

  LuaWriter luaWriter;
  // Start the lua project document
  luaWriter.WriteStartDocument();

  luaWriter.WriteCommentSeparation(projFile);
  luaWriter.WriteCommentLine("", projFile);
  luaWriter.WriteCommentSeparation(projFile);
  int assetNum = 0;
  luaWriter.WriteDeclareTable("project", projFile);
  luaWriter.WriteDeclareTable("assets", projFile);
  for (const auto &asset : assetIds) {
    luaWriter.WriteStartTable(assetNum, false, projFile);
    luaWriter.WriteKeyAndQuotedValue("asset_id", asset, projFile, true);
    luaWriter.WriteKeyAndQuotedValue("file_path", assetFilepaths[assetNum],
                                     projFile, true);
    luaWriter.WriteEndTable(false, projFile);
    assetNum++;
  }
  // End that table
  luaWriter.WriteEndTable(false, projFile);

  fs::path filepath(filename);
  filepath.replace_extension(".map");

  luaWriter.WriteDeclareTable("maps", projFile);
  luaWriter.WriteStartTable(0, false, projFile);
  luaWriter.WriteKeyAndQuotedValue("file_path", filepath.string(), projFile);
  luaWriter.WriteEndTableWithSeparator(false, projFile);
  luaWriter.WriteEndTable(false, projFile);

  luaWriter.WriteDeclareTable("canvas", projFile);
  luaWriter.WriteKeyAndUnquotedValue("canvas_width", canvasWidth, projFile,
                                     false, false);
  luaWriter.WriteKeyAndUnquotedValue("canvas_height", canvasHeight, projFile,
                                     false, false);
  luaWriter.WriteKeyAndUnquotedValue("tile_size", tileSize, projFile, false,
                                     true);

  luaWriter.WriteEndTable(false, projFile);
  luaWriter.WriteEndTable(false, projFile);

  // Check to see if there are no indents remaining
  luaWriter.WriteEndDocument(projFile);

  // Close the project file
  projFile.close();

  std::fstream mapFile;

  mapFile.open(filepath, std::ios::out);

  if (!mapFile.is_open()) {
    logger.Err("FILELOADER__LINE__313: Unable to open[{0}] for saving " +
               filepath.u8string());
    return;
  }

  SaveMap(filepath);

  if (!Registry::Instance().DoesGroupExist("colliders"))
    return;

  std::fstream colliderFile;
  std::string newFile = filepath.stem().string() += "_colliders.map";

  filepath.replace_filename(newFile);

  colliderFile.open(filepath, std::ios::out);

  if (!colliderFile.is_open()) {
    logger.Err("FILELOADER__LINE__327: Unable to open[{0}] for saving " +
               filepath.u8string());
    return;
  }
  logger.Log("Collider: " + filepath.string());

  SaveColliders(filepath);
}

void FileLoader::SaveToLuaTable(const std::string &filename,
                                std::vector<std::string> &assetIds,
                                std::vector<std::string> &assetFilepaths,
                                const int &tileSize) {
  std::fstream projFile;
  projFile.open(filename, std::ios::out | std::ios::trunc);

  if (!projFile.is_open()) {
    logger.Err("FILELOADER__LINE__223: Unable to open[{0}] for saving " +
               filename);
    return;
  }

  LuaWriter luaWriter;
  // Start the lua project document
  luaWriter.WriteStartDocument();

  luaWriter.WriteCommentSeparation(projFile);
  luaWriter.WriteCommentLine("", projFile);
  luaWriter.WriteCommentSeparation(projFile);

  luaWriter.WriteWords("return {", projFile, true);
  luaWriter.WriteKeyAndUnquotedValue("id", "id", projFile, false, false);
  luaWriter.WriteKeyAndUnquotedValue("name", "", projFile, false, false);
  luaWriter.WriteKeyAndUnquotedValue("tileWidth", tileSize, projFile, false,
                                     false);
  luaWriter.WriteKeyAndUnquotedValue("tileHeight", tileSize, projFile, false,
                                     false);
  luaWriter.WriteDeclareTable("on_wake", projFile);
  luaWriter.WriteEndTable(false, projFile);
  luaWriter.WriteDeclareTable("actions", projFile);
  luaWriter.WriteEndTable(false, projFile);
  luaWriter.WriteDeclareTable("trigger_types", projFile);
  luaWriter.WriteEndTable(false, projFile);
  luaWriter.WriteDeclareTable("triggers", projFile);
  luaWriter.WriteEndTable(false, projFile);
  luaWriter.WriteDeclareTable("tiles", projFile);

  if (Registry::Instance().DoesGroupExist("tiles")) {
    int i = 1;
    for (const auto &tile : Registry::Instance().GetEntitiesByGroup("tiles")) {
      luaWriter.WriteStartTable(i, false, projFile);
      luaWriter.WriteDeclareTable("components", projFile);
      if (tile.HasComponent<TransformComponent>()) {
        const auto &transform = tile.GetComponent<TransformComponent>();

        luaWriter.WriteDeclareTable("transform", projFile);
        luaWriter.WriteDeclareTable("position", projFile);
        luaWriter.WriteKeyAndValue("x", transform.position.x, false, projFile);
        luaWriter.WriteKeyAndValue("y", transform.position.y, true, projFile);
        luaWriter.WriteEndTable(true, projFile);
        luaWriter.WriteDeclareTable("scale", projFile);
        luaWriter.WriteKeyAndValue("x", transform.scale.x, false, projFile);
        luaWriter.WriteKeyAndValue("y", transform.scale.y, true, projFile);
        luaWriter.WriteEndTable(true, projFile);
        luaWriter.WriteKeyAndUnquotedValue("rotation", transform.rotation,
                                           projFile, false, false);
        luaWriter.WriteEndTable(false, projFile);
      }

      if (tile.HasComponent<SpriteComponent>()) {
        const auto &sprite = tile.GetComponent<SpriteComponent>();

        std::string fixed = "false";

        if (sprite.isFixed)
          fixed = "true";

        luaWriter.WriteDeclareTable("sprite", projFile);
        luaWriter.WriteKeyAndQuotedValue("asset_id", sprite.assetId, projFile);
        luaWriter.WriteKeyAndValue("width", sprite.width, false, projFile);
        luaWriter.WriteKeyAndValue("height", sprite.height, false, projFile);
        luaWriter.WriteKeyAndValue("z_index", sprite.zIndex, false, projFile);
        luaWriter.WriteKeyAndValue("is_fixed", fixed, true, projFile);
        luaWriter.WriteDeclareTable("src_rect", projFile);
        luaWriter.WriteKeyAndValue("x", sprite.srcRect.x, false, projFile);
        luaWriter.WriteKeyAndValue("y", sprite.srcRect.y, true, projFile);
        luaWriter.WriteEndTable(true, projFile);
        luaWriter.WriteDeclareTable("offset", projFile);
        luaWriter.WriteKeyAndValue("x", sprite.offset.x, false, projFile);
        luaWriter.WriteKeyAndValue("y", sprite.offset.y, true, projFile);
        luaWriter.WriteEndTable(true, projFile);
        luaWriter.WriteEndTable(false, projFile);
      }

      if (tile.HasComponent<BoxColliderComponent>()) {
        const auto &boxCollider = tile.GetComponent<BoxColliderComponent>();

        luaWriter.WriteDeclareTable("box_collider", projFile);
        luaWriter.WriteKeyAndValue("width", boxCollider.width, false, projFile);
        luaWriter.WriteKeyAndValue("height", boxCollider.height, true,
                                   projFile);
        luaWriter.WriteDeclareTable("offset", projFile);
        luaWriter.WriteKeyAndValue("x", boxCollider.offset.x, false, projFile);
        luaWriter.WriteKeyAndValue("y", boxCollider.offset.y, true, projFile);
        luaWriter.WriteEndTable(true, projFile);
        luaWriter.WriteKeyAndUnquotedValue("is_collider", "true", projFile);
        luaWriter.WriteKeyAndUnquotedValue("is_trigger", "false", projFile);
        luaWriter.WriteEndTable(false, projFile);
      }

      if (tile.HasComponent<AnimationComponent>()) {
        const auto &animation = tile.GetComponent<AnimationComponent>();

        std::string vertical = "false";
        std::string looped = "false";
        if (animation.vertical)
          vertical = "true";
        if (animation.isLooped)
          looped = "true";

        luaWriter.WriteDeclareTable("animation", projFile);
        luaWriter.WriteKeyAndValue("num_frames", animation.numFrames, false,
                                   projFile);
        luaWriter.WriteKeyAndValue("frame_speed", animation.frameSpeedRate,
                                   false, projFile);
        luaWriter.WriteKeyAndValue("vertical", vertical, false, projFile);
        luaWriter.WriteKeyAndValue("looped", looped, false, projFile);
        luaWriter.WriteKeyAndValue("frame_offset", animation.frameOffset, true,
                                   projFile);
        luaWriter.WriteEndTable(false, projFile);
      }

      luaWriter.WriteEndTable(false, projFile);
      luaWriter.WriteEndTable(false, projFile);
      i++;
    }
  }

  // Loop through all the tiles
  luaWriter.WriteEndTable(false, projFile);
  luaWriter.WriteEndTable(false, projFile);
  // No trailing "end" — the document is a plain `return { ... }` table, and a
  // stray `end` made the exported file a Lua syntax error.
  luaWriter.WriteEndDocument(projFile);
  projFile.close();
}
