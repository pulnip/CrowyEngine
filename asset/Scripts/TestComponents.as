class Component{
    EntityHandle self;

    Component(EntityHandle handle){
        self = handle;
    }

    CharacterController@ getCharacterController(){
        return self.getCharacterController();
    }
}

class TestComponent: Component{
    TestComponent(EntityHandle handle){
        super(handle);
    }

    void onStart(){
        println("started!");
    }

    void onUpdate(float dt){
        if(isAction("jump"))
            println("space key pressed");
    }

    void onFinish(){
        println("finished!");
    }
}