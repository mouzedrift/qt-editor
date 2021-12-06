#include "model.hpp"
#include <optional>
#include <fstream>

static std::optional<std::string> LoadFileToString(const std::string& fileName)
{
    std::ifstream fileStream(fileName.c_str());
    if (!fileStream)
    {
        return {};
    }
    std::string outputStr;

    fileStream.seekg(0, std::ios::end);
    outputStr.reserve(static_cast<size_t>(fileStream.tellg()));
    fileStream.seekg(0, std::ios::beg);

    outputStr.assign((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    return outputStr;
}

static jsonxx::Array ReadArray(jsonxx::Object& o, const std::string& key)
{
    if (!o.has<jsonxx::Array>(key))
    {
        throw JsonKeyNotFoundException(key);
    }
    return o.get<jsonxx::Array>(key);
}

static jsonxx::Object ReadObject(jsonxx::Object& o, const std::string& key)
{
    if (!o.has<jsonxx::Object>(key))
    {
        throw JsonKeyNotFoundException(key);
    }
    return o.get<jsonxx::Object>(key);
}

static int ReadNumber(jsonxx::Object& o, const std::string& key)
{
    if (!o.has<jsonxx::Number>(key))
    {
        throw JsonKeyNotFoundException(key);
    }
    return static_cast<int>(o.get<jsonxx::Number>(key));
}

static std::string ReadString(jsonxx::Object& o, const std::string& key)
{
    if (!o.has<jsonxx::String>(key))
    {
        throw JsonKeyNotFoundException(key);
    }
    return o.get<jsonxx::String>(key);
}

static std::string ReadStringOptional(jsonxx::Object& o, const std::string& key)
{
    if (!o.has<jsonxx::String>(key))
    {
        return "";
    }
    return o.get<jsonxx::String>(key);
}

static bool ReadBool(jsonxx::Object& o, const std::string& key)
{
    if (!o.has<jsonxx::Boolean>(key))
    {
        throw JsonKeyNotFoundException(key);
    }
    return o.get<jsonxx::Boolean>(key);
}

static std::vector<EnumOrBasicTypeProperty> ReadObjectStructureProperties(jsonxx::Array& enumAndBasicTypes)
{
    std::vector<EnumOrBasicTypeProperty> properties;
    for (size_t j = 0; j < enumAndBasicTypes.size(); j++)
    {
        jsonxx::Object enumOrBasicType = enumAndBasicTypes.get<jsonxx::Object>(j);
        EnumOrBasicTypeProperty tmpEnumOrBasicTypeProperty;
        tmpEnumOrBasicTypeProperty.mName = ReadString(enumOrBasicType, "name");
        tmpEnumOrBasicTypeProperty.mType = ReadString(enumOrBasicType, "Type");
        tmpEnumOrBasicTypeProperty.mVisible = ReadBool(enumOrBasicType, "Visible");
        properties.push_back(tmpEnumOrBasicTypeProperty);
    }
    return properties;
}

static UP_ObjectStructure ReadObjectStructure(jsonxx::Object& objectStructure)
{
    auto tmpObjectStructure = std::make_unique<ObjectStructure>();
    tmpObjectStructure->mName = ReadString(objectStructure, "name");

    jsonxx::Array enumAndBasicTypes = ReadArray(objectStructure, "enum_and_basic_type_properties");
    tmpObjectStructure->mEnumAndBasicTypeProperties = ReadObjectStructureProperties(enumAndBasicTypes);

    return tmpObjectStructure;
}

std::vector<UP_ObjectProperty> Model::ReadProperties(const ObjectStructure* pObjStructure, jsonxx::Object& properties)
{
    std::vector<UP_ObjectProperty> tmpProperties;
    for (const EnumOrBasicTypeProperty& property : pObjStructure->mEnumAndBasicTypeProperties)
    {
        const FoundType foundTypes = FindType(property.mType);
        if (!foundTypes.mEnum && !foundTypes.mBasicType)
        {
            // corrupted schema type name has no definition
            throw ObjectPropertyTypeNotFoundException(property.mName, property.mType);
        }

        auto tmpProperty = std::make_unique<ObjectProperty>();
        tmpProperty->mName = property.mName;
        tmpProperty->mTypeName = property.mType;
        tmpProperty->mVisible = property.mVisible;
        if (foundTypes.mBasicType)
        {
            tmpProperty->mType = ObjectProperty::Type::BasicType;
            tmpProperty->mBasicTypeValue = ReadNumber(properties, property.mName);
        }
        else
        {
            tmpProperty->mType = ObjectProperty::Type::Enumeration;
            tmpProperty->mEnumValue = ReadString(properties, property.mName);
        }

        tmpProperties.push_back(std::move(tmpProperty));
    }
    return tmpProperties;
}


void Model::LoadJson(const std::string& jsonFile)
{
    std::optional<std::string> jsonString = LoadFileToString(jsonFile);
    if (!jsonString)
    {
        throw IOReadException(jsonFile);
    }

    jsonxx::Object root;
    if (!root.parse(*jsonString))
    {
        throw InvalidJsonException();
    }
    
    mMapInfo.mApiVersion = ReadNumber(root, "api_version");
    mMapInfo.mGame = ReadString(root, "game");

    jsonxx::Object map = ReadObject(root, "map");

    mMapInfo.mPathBnd = ReadString(map, "path_bnd");
    mMapInfo.mPathId = ReadNumber(map, "path_id");
    mMapInfo.mXGridSize = ReadNumber(map, "x_grid_size");
    mMapInfo.mXSize = ReadNumber(map, "x_size");
    mMapInfo.mYGridSize = ReadNumber(map, "y_grid_size");
    mMapInfo.mYSize = ReadNumber(map, "y_size");

    mSchema = ReadObject(root, "schema");

    jsonxx::Array basicTypes = ReadArray(mSchema, "object_structure_property_basic_types");
    for (size_t i = 0; i < basicTypes.size(); i++)
    {
        jsonxx::Object basicType = basicTypes.get<jsonxx::Object>(i);
        auto tmpBasicType = std::make_unique<BasicType>();
        tmpBasicType->mName = ReadString(basicType, "name");
        tmpBasicType->mMaxValue = ReadNumber(basicType, "max_value");
        tmpBasicType->mMinValue = ReadNumber(basicType, "min_value");
        mBasicTypes.push_back(std::move(tmpBasicType));
    }

    jsonxx::Array enums = ReadArray(mSchema, "object_structure_property_enums");
    for (size_t i = 0; i < enums.size(); i++)
    {
        jsonxx::Object enumObject = enums.get<jsonxx::Object>(i);
        auto tmpEnum = std::make_unique<Enum>();
        tmpEnum->mName = ReadString(enumObject, "name");

        jsonxx::Array enumValuesArray = ReadArray(enumObject, "values");
        for (size_t j = 0; j < enumValuesArray.size(); j++)
        {
            tmpEnum->mValues.push_back(enumValuesArray.get<jsonxx::String>(j));
        }
        mEnums.push_back(std::move(tmpEnum));
    }

    jsonxx::Array objectStructures = ReadArray(mSchema, "object_structures");
    for (size_t i = 0; i < objectStructures.size(); i++)
    {
        jsonxx::Object objectStructure = objectStructures.get<jsonxx::Object>(i);
        auto tmpObjectStructure = ReadObjectStructure(objectStructure);
        mObjectStructures.push_back(std::move(tmpObjectStructure));
    }

    jsonxx::Array cameras = ReadArray(map, "cameras");
    for (size_t i = 0; i < cameras.size(); i++)
    {
        jsonxx::Object camera = cameras.get<jsonxx::Object>(i);

        auto tmpCamera = std::make_unique<Camera>();
        tmpCamera->mId = ReadNumber(camera, "id");
        tmpCamera->mName = ReadString(camera, "name");
        tmpCamera->mX = ReadNumber(camera, "x");
        tmpCamera->mY = ReadNumber(camera, "y");

        tmpCamera->mCameraImageandLayers.mCameraImage = ReadStringOptional(camera, "image");
        tmpCamera->mCameraImageandLayers.mForegroundLayer = ReadStringOptional(camera, "foreground_layer");
        tmpCamera->mCameraImageandLayers.mBackgroundLayer = ReadStringOptional(camera, "background_layer");
        tmpCamera->mCameraImageandLayers.mForegroundWellLayer = ReadStringOptional(camera, "foreground_well_layer");
        tmpCamera->mCameraImageandLayers.mBackgroundWellLayer = ReadStringOptional(camera, "background_well_layer");

        if (camera.has<jsonxx::Array>("map_objects"))
        {
            jsonxx::Array mapObjects = ReadArray(camera, "map_objects");

            for (size_t j = 0; j < mapObjects.size(); j++)
            {
                jsonxx::Object mapObject = mapObjects.get<jsonxx::Object>(j);
                auto tmpMapObject = std::make_unique<MapObject>();
                tmpMapObject->mName = ReadString(mapObject, "name");
                tmpMapObject->mObjectStructureType = ReadString(mapObject, "object_structures_type");

                if (mapObject.has<jsonxx::Object>("properties"))
                {
                    const ObjectStructure* pObjStructure = nullptr;
                    for (const auto& objStruct : mObjectStructures)
                    {
                        if (objStruct->mName == tmpMapObject->mObjectStructureType)
                        {
                            pObjStructure = objStruct.get();
                            break;
                        }
                    }

                    if (!pObjStructure)
                    {
                        throw JsonKeyNotFoundException(tmpMapObject->mObjectStructureType);
                    }

                    jsonxx::Object properties = ReadObject(mapObject, "properties");
                    tmpMapObject->mProperties = ReadProperties(pObjStructure, properties);
                }

                tmpCamera->mMapObjects.push_back(std::move(tmpMapObject));
            }
        }
        mCameras.push_back(std::move(tmpCamera));
    }


    jsonxx::Object collisionObject = ReadObject(map, "collisions");
    jsonxx::Array collisionsArray = ReadArray(collisionObject, "items");
    mCollisionStructureSchema = ReadArray(collisionObject, "structure");

    mCollisionStructure= std::make_unique<ObjectStructure>();
    mCollisionStructure->mName = "Collision";
    mCollisionStructure->mEnumAndBasicTypeProperties = ReadObjectStructureProperties(mCollisionStructureSchema);

    for (size_t i = 0; i < collisionsArray.size(); i++)
    {
        jsonxx::Object collision = collisionsArray.get<jsonxx::Object>(i);

        auto tmpCollision = std::make_unique<CollisionObject>();
        tmpCollision->mProperties = ReadProperties(mCollisionStructure.get(), collision);
        mCollisions.push_back(std::move(tmpCollision));
    }

}

std::string Model::ToJson() const
{
    jsonxx::Object root;

    root << "api_version" << mMapInfo.mApiVersion;
    root << "game" << mMapInfo.mGame;

    jsonxx::Object map;
    map << "path_bnd" << mMapInfo.mPathBnd;
    map << "path_id" << mMapInfo.mPathId;
    map << "x_grid_size" << mMapInfo.mXGridSize;
    map << "x_size" << mMapInfo.mXSize;
    map << "y_grid_size" << mMapInfo.mYGridSize;
    map << "y_size" << mMapInfo.mYSize;

    jsonxx::Array cameras;
    for (auto& camera : mCameras)
    {
        jsonxx::Object camObj;
        camObj << "id" << camera->mId;
        camObj << "name" << camera->mName;
        camObj << "x" << camera->mX;
        camObj << "y" << camera->mY;

        if (!camera->mCameraImageandLayers.mCameraImage.empty())
        {
            camObj << "image" << camera->mCameraImageandLayers.mCameraImage;
        }

        if (!camera->mCameraImageandLayers.mForegroundLayer.empty())
        {
            camObj << "foreground_layer" << camera->mCameraImageandLayers.mForegroundLayer;
        }

        if (!camera->mCameraImageandLayers.mBackgroundLayer.empty())
        {
            camObj << "background_layer" << camera->mCameraImageandLayers.mBackgroundLayer;
        }

        if (!camera->mCameraImageandLayers.mForegroundWellLayer.empty())
        {
            camObj << "foreground_well_layer" << camera->mCameraImageandLayers.mForegroundWellLayer;
        }

        if (!camera->mCameraImageandLayers.mBackgroundWellLayer.empty())
        {
            camObj << "background_well_layer" << camera->mCameraImageandLayers.mBackgroundWellLayer;
        }

        jsonxx::Array mapObjects;
        for (auto& mapObject : camera->mMapObjects)
        {
            jsonxx::Object mapObj;
            mapObj << "name" << mapObject->mName;
            mapObj << "object_structures_type" << mapObject->mObjectStructureType;
            jsonxx::Object propertiesObject;
            for (auto& property : mapObject->mProperties)
            {
                switch (property->mType)
                {
                case ObjectProperty::Type::BasicType:
                    propertiesObject << property->mName << property->mBasicTypeValue;
                    break;

                case ObjectProperty::Type::Enumeration:
                    propertiesObject << property->mName << property->mEnumValue;
                    break;
                }
            }
            mapObj << "properties" << propertiesObject;
            mapObjects << mapObj;
        }
        camObj << "map_objects" << mapObjects;

        cameras << camObj;
    }

    jsonxx::Object collisionsObject;
    jsonxx::Array collisionItems;
    for (auto& collision : mCollisions)
    {
        jsonxx::Object collisionObj;
        for (auto& property : collision->mProperties)
        {
            switch (property->mType)
            {
            case ObjectProperty::Type::BasicType:
                collisionObj << property->mName << property->mBasicTypeValue;
                break;

            case ObjectProperty::Type::Enumeration:
                collisionObj << property->mName << property->mEnumValue;
                break;
            }
        }
        collisionItems << collisionObj;
    }

    collisionsObject << "items" << collisionItems;
    collisionsObject << "structure" << mCollisionStructureSchema;
    map << "collisions" << collisionsObject;

    map << "cameras" << cameras;
    root << "map" << map;

    root << "schema" << mSchema;

    return root.json();
}
