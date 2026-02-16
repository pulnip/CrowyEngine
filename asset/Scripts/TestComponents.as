class TestComponent{
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