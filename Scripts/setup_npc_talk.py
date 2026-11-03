import unreal
# places/updates one NPCTalker per city NPC (press E to talk). edit LINES and re-run.
# run in NightCityCreator: exec(open(unreal.Paths.project_dir()+'Scripts/setup_npc_talk.py').read())
LINES = {
    'Jolleen': [
        "Is that a costume or do you just live in Bushwick?",
        "I paid $40 for this cover and the club looks empty.",
        "My roommate's been inside that club since LAST Halloween.",
        "The L train is down. Again. Spooky.",
        "Don't go in there. Unless you can dance. Can you dance?",
        "I'm dressed as a landlord. Scariest costume here.",
    ],
    'Jackie': [
        "Have you seen my friend? Vampire, sparkly, owes me $20.",
        "This bodega cat looked me dead in the eyes and said 'leave'.",
        "The bass from that club is making my teeth hum.",
        "I've been looking for parking since 9pm.",
        "Someone told me the DJ hasn't blinked in three hours.",
        "My oat milk latte was $9. THAT'S the horror.",
    ],
    'Bobby': [
        "I'm not walking slow, I'm pacing myself.",
        "Back in my day, Bushwick was just warehouses. Now it's haunted warehouses.",
        "You hear that music? Me neither. Keep walking.",
        "I came for the free candy. There is no free candy.",
        "Every year someone goes in that club and comes out a better dancer. Or doesn't come out.",
        "Nice costume. Oh. That's just your outfit. Sorry.",
    ],
}
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
cls = unreal.load_class(None, '/Script/DanceDanceDie.NPCTalker')
assert cls, 'NPCTalker class missing: rebuild C++ first'
acts = {a.get_actor_label(): a for a in EAS.get_all_level_actors()}
for name, lines in LINES.items():
    npc = acts.get('NPC_' + name)
    assert npc, 'missing NPC_' + name
    lab = 'NPC_%s_Talk' % name
    t = acts.get(lab) or EAS.spawn_actor_from_class(cls, npc.get_actor_location())
    t.set_actor_label(lab)
    t.set_folder_path('NPCs')
    t.set_editor_property('npc', npc)
    t.set_editor_property('speaker_name', name)
    t.set_editor_property('lines', lines)
    unreal.log('NPCTALK %s ok (%d lines)' % (lab, len(lines)))
unreal.EditorLevelLibrary.save_current_level()
