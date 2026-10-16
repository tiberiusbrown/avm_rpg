<?xml version='1.0' encoding='utf-8'?>
<tileset version="1.10" tiledversion="1.11.2" name="world" tilewidth="8" tileheight="8" tilecount="5" columns="5">
  <image source="../../assets_src/tiles.png" width="40" height="8" />
  <tile id="0">
    <properties>
      <property name="runtime_id" value="0" type="int" />
      <property name="symbol" value="GRASS" type="string" />
      <property name="solid" value="false" type="bool" />
      <property name="portal" value="false" type="bool" />
    </properties>
  </tile>
  <tile id="1">
    <properties>
      <property name="runtime_id" value="1" type="int" />
      <property name="symbol" value="WALL" type="string" />
      <property name="solid" value="true" type="bool" />
      <property name="portal" value="false" type="bool" />
    </properties>
  </tile>
  <tile id="2">
    <properties>
      <property name="runtime_id" value="2" type="int" />
      <property name="symbol" value="WATER" type="string" />
      <property name="solid" value="true" type="bool" />
      <property name="portal" value="false" type="bool" />
    </properties>
  </tile>
  <tile id="3">
    <properties>
      <property name="runtime_id" value="3" type="int" />
      <property name="symbol" value="PORTAL" type="string" />
      <property name="solid" value="false" type="bool" />
      <property name="portal" value="true" type="bool" />
    </properties>
  </tile>
  <tile id="4">
    <properties>
      <property name="runtime_id" value="4" type="int" />
      <property name="symbol" value="PATH" type="string" />
      <property name="solid" value="false" type="bool" />
      <property name="portal" value="false" type="bool" />
    </properties>
  </tile>
</tileset>