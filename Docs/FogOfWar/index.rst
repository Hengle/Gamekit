Fog Of War
==========

Relevant classes:

* :cpp:class:`AGKFogOfWarVolume`
* :cpp:class:`UGKFogOfWarComponent`

.. image :: /_static/FogOfWar.PNG

Fog of war can easily be added to your level by adding a :cpp:class:`AGKFogOfWarVolume` to your level and adding a :cpp:class:`UGKFogOfWarComponent` to every actor that
participate in the fog of war.

Active Fog of war actors paint their vision on a render target texture.
The texture is used to know if other actors are visible or not.

The Collision channel ``FoWObstacle`` is used to control if the actor is blocking vision or not.
By default all actors block visions.

FoW is using a custom collision channel, it is expected to be using ``ECC_GameTraceChannel1`` by default,
the exact collision channel is customazible using the Collision trace property in ``FogOfWarVolume``

.. code-block:: ini

   [/Script/Engine.CollisionProfile]
   +DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Block,bTraceType=True,bStaticObject=False,Name="FoWObstacle")

The fog of ware is rendered on screen using a post process volume.
If the post process method is used either the camera component must use the provided post process material or
a post process volume must be added to the level with the appropriate post process material.

.. note::

    Adding the FoW post processing step to the camera itself will prevent issues where the camera comes out of the PostProcessVolume
    when walking close to the edge of the map.

.. note::

    To have the fog be displayed correctly in the editor, make sure the ``FoWParameters`` defaults are matching your FogOfWarVolume and texture.

        * ``FoWParameters.MapSize = FogOfWarVolume.Scale * FogOfWarVolume.BrushSize (default = 200)``
        * ``FoWParameters.TextureSize = FoWParameters.MapSize * FogOfWarVolume.TextureScale``

.. note:: Decal Rendering

   A decal Component on the :cpp:class:`AGKFogOfWarVolume` can be used to render the fog of war on screen instead of using a post processing step.
   This method is not advised because it might adversly impact other part of the game that are using decals (cursor, etc..)

   Additionally in the case of a game with factions it is easier to tweak the ``CameraComponent|PostProcessingMaterial`` to use the faction fog of war than to
   modify the globally unique :cpp:class:`AGKFogOfWarVolume`.


.. note::

   UnrealEngine implements ``UAISense_Sight`` (`doc`_) which requires sightable targets to implement ``IAISightTargetInterface``.
   The implementation is different an tries to limit the number of trace done.
   As a result the full line of sight is not drawn. It might be a path worth investigating if you find ``AGKFogOfWarVolume``
   to be too expensive.


.. _doc: https://docs.unrealengine.com/4.26/en-US/API/Runtime/AIModule/Perception/UAISense_Sight/


Exploration
-----------

Exploration is managed through another render target which has the current vision added on every FoWVolume tick.


Line of Sight
-------------

The fog of war can be used for stealth games; line of sights are cast to determine which part of the map is visible.



Idea
----


I thought I could improve my fog of war by using a point light for the tracing and draw the point light on a render target but it does not seem possible in UE4.
There are 3 light channels but they cant be rendered separately or I have not found a way to do so