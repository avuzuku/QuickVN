// Start background music (looping) and initial scene
engine.playMusic("assets/music_main.wav", true);
engine.showBackground("assets/bg_school.png");
engine.showCharacter("assets/alice_normal.png");

// Play school bell effect
engine.playSound("assets/bell.wav");

engine.say("Alice", "Good morning! Did you hear that chime? The morning bell just rang.");
engine.say("Alice", "We have about ten minutes before the teacher arrives.");
engine.say("Alice", "Where do you want to spend our break?");

// Interactive branching with different scenes and audio effects
engine.showChoices([
    "Go inside the Classroom",
    "Sneak onto the Rooftop",
    "Skip class and go Home"
], (choice) => {
    // Play button click sound effect
    engine.playSound("assets/click.wav");

    if (choice === 0) {
        // Branch 1: Classroom
        engine.showBackground("assets/bg_classroom.png");
        engine.showCharacter("assets/alice_happy.png");

        engine.say("Alice", "Sensible choice! Let's grab the two desks by the window.");
        engine.say("Alice", "I brought some warm tea. We can review our notes in peace.");
    } 
    else if (choice === 1) {
        // Branch 2: Rooftop
        engine.showBackground("assets/bg_rooftop.png");
        engine.showCharacter("assets/alice_surprised.png");

        engine.say("Alice", "Wait, the rooftop?! Isn't that door supposed to be locked?");
        
        // Change expression to happy
        engine.showCharacter("assets/alice_happy.png");
        engine.say("Alice", "Well... who cares! The breeze up here feels amazing.");
    } 
    else {
        // Branch 3: Go home (Dramatic pause, stop music)
        engine.stopMusic();
        engine.showCharacter("assets/alice_surprised.png");

        engine.say("Alice", "Wait, are you serious? Leaving school right now?");
        engine.say("Alice", "The hallway went completely silent...");
        engine.say("Alice", "I guess today is an adventure day then.");
    }

    // Common epilogue
    engine.say("Alice", "Test complete! Music streaming, sound effects, sprite swapping, and branching are all working.");
});