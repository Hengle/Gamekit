Tips & Tricks
=============

Useful examples https://www.ue4community.wiki/logging-lgpidy6i

Logging values
--------------

.. code-block:: cpp

    FName name;
    UE_LOG(LogChessy, Verbose, TEXT("Creating a new render target for %s"), *name.ToString());

Finding an instance by type/class

.. code-block:: cpp

    TSubclassOf<AFogOfWarVolume> classToFind = AFogOfWarVolume::StaticClass();
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), classToFind, OutActors);


Doxygen Friendly
----------------

.. code-block:: cpp

    // Do not put arguments to UCLASS, doxygen does not like it, the whole header file will get ignored
    UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
    class GAMEKIT_API UFogOfWarComponent : public UActorComponent
    {
        //
    };


.. code-block:: cpp

	// UE_LOG(LogTemp, Warning, TEXT("%fx%f"), CamDir.X, CamDir.Y);


	InputComponent->BindAction("SetDestination", IE_Pressed, this, &AChessyPlayerController::OnSetDestinationPressed);
	InputComponent->BindAction("SetDestination", IE_Released, this, &AChessyPlayerController::OnSetDestinationReleased);

	// support touch devices
	InputComponent->BindTouch(EInputEvent::IE_Pressed, this, &AChessyPlayerController::MoveToTouchLocation);
	InputComponent->BindTouch(EInputEvent::IE_Repeat, this, &AChessyPlayerController::MoveToTouchLocation);

	InputComponent->BindAction("ResetVR", IE_Pressed, this, &AChessyPlayerController::OnResetVR);

    UFUNCTION(BlueprintCallable)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GameSpeed)

    CameraBoom->SetupAttachment(RootScene);


Generating Textures
-------------------

You can use blender to generate texture and use an orthographic camera to export the render as texture.

* Create square plan with the material we want to export
* Set Output Resolution to 1024x1024
* Set Camera Type to Orthographic
* Set Camera Orthographic scale to match the plan
* Render (F12), save render to file

.. image:: /_static/Blender.png

.. image:: /_static/OrthographicRender.png


Others
------

.. toctree::

   Controllers/index
   Cameras/index
   Minimaps/index
   Shaders/index
