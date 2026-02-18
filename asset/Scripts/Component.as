class Component{
    EntityHandle self;

    Component(EntityHandle handle){
        self = handle;
    }

    CharacterController@ getCharacterController(){
        return self.getCharacterController();
    }
}
