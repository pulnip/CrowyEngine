class TestComponent: Component{
    TestComponent(EntityHandle handle){
        super(handle);
    }

    void onStart(){
        println("Start");
    }

    void onUpdate(float dt){
        println("Update");
    }

    void onFinish(){
        println("Finish");
    }
}

class MoveComponent: Component{
    TransformComponent@ transform;
    float v = 5.0f;

    MoveComponent(EntityHandle handle){
        super(handle);

        @transform = getTransformComponent();
    }

    void onUpdate(float dt){
        if(isAction("MoveLeft"))
            transform.position.x -= v * dt;
        if(isAction("MoveRight"))
            transform.position.x += v * dt;
        if(isAction("MoveDown"))
            transform.position.y -= v * dt;
        if(isAction("MoveUp"))
            transform.position.y += v * dt;
        if(isAction("MoveBackward"))
            transform.position.z -= v * dt;
        if(isAction("MoveForward"))
            transform.position.z += v * dt;
    }
}