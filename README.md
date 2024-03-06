# Runtime editable Landscape

The runtime editable Landscape is a Unreal Engine 5 Plugin that aims to make Landscape editable at runtime. I developed it for the city builder [BootyIsland](https://bootyisland.itch.io/bootyisland), I'm currently working on.

## Features

* Edit the height of the landscape at runtime
* Edit vertex colors at runtime

## Usage

### Add a runtime editable Landscape to the world
1. Add an actor of type `Runtime Landscape` to your World
2. Int the Runtime Landscape pick the `Parent Landscape`
3. (Optional but recommended) set the `Is Editor Only Actor` on the original Landscape

### Add holes

Holes in the parent Landscape (i.E. for cave entries) are not used, however holes can be added by placing `Landcape Hole` actors at the desired location and selecting the affected landscapes in the `Affected Landscapes` property.

### Edit the Landscape at runtime
...

## State of development

At the moment this plugin is still in prototyping phase. There is some functionality available, but I do not recommend using it for your own project, yet.

### TODOs

* Holes
    * Rotation of box holes
    * Fix relink when components are created
* Use Landscape layer data
* Adjust foliage
* Optimize Performance
* Clean up code

### Known issues

