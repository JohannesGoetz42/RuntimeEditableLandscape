# Runtime editable Landscape

The runtime editable Landscape is a Unreal Engine 5 Plugin that aims to make Landscape editable at runtime. I developed it for the city builder [BootyIsland](https://bootyisland.itch.io/bootyisland), I'm currently working on.
It is designed to do updates to the Landscape during runtime (i.E. when creating a building), but not on every frame.

## State of development

At the moment this plugin is a work in Progress. There is some functionality available, but I do not recommend using it for your own project, yet.

## Features
* Edit the height of the landscape at runtime
* Edit vertex colors at runtime
* Edit holes at runtime

## Usage

### Add a runtime editable Landscape to the world
1. Add an actor of type `Runtime Landscape` to your World
2. Int the Runtime Landscape pick the `Parent Landscape`
3. (Optional but recommended) set the `Is Editor Only Actor` on the original Landscape

### Edit the Landscape at runtime
To edit the landscape, you can place `Landscape layer actors` in the world and adjust the parameters in the `Landscape layer component`. You can also add this component to your own actors.

### Holes
Holes in the parent Landscape (i.E. for cave entries) are not used, however holes can be added with `Landcape Layers` (see [Edit the Landscape at runtime](###edit-the-landscape-at-runtime))

### TODOs

* Adjust foliage
* Optimize Performance
    * Multithreading!
* Clean up code
