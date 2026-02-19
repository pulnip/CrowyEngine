class Component{
    EntityHandle self;

    Component(EntityHandle handle){
        self = handle;
    }

    TransformComponent@ getTransformComponent(){
        return self.getTransformComponent();
    }

    CharacterController@ getCharacterController(){
        return self.getCharacterController();
    }
}
