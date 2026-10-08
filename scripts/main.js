// Display scene
engine.showBackground("assets/bg_school.png");
engine.showCharacter("assets/character.png");

// Dialogue
engine.say("Alice", "Hello! Welcome to QuickVN.");
engine.say("Alice", "This visual novel engine is built with C++ and QuickJS.");
engine.say("Alice", "All scripting is handled via embedded JavaScript.");

// Scene transition
engine.showBackground("assets/bg_room.png");
engine.say("Alice", "Location change completed successfully!");