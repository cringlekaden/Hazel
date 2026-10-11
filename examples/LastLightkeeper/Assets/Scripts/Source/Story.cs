namespace LastLightkeeper {
    // Character and puzzle writing stays in this game, beside its authored scenes.
    public static class Story {
        public static readonly string[] Arrival={
            "ORRIN\nDon't step off the landing. The bridge motor's dead.\nThat mirror can feed it from the harbor lamp.",
            "IONA\nFather's left the cover off again. Salt in every bolt.\nStay with the boat, Orrin. I'll get you a signal."
        };
        public static readonly string[] Ferryman={
            "ORRIN\nTwo people were waving from the survey ledge.\nI can't take the boat through that reef blind.",
            "ORRIN\nMara went up the cliff path before the steps fell.\nShe dropped her wrench in the grove. Big yellow one.\nYou'll need it for the pump."
        };
        public static readonly string[] Grove={
            "MARA'S NOTE\nLens safe on the pump platform. Drain's seized.\nWrench by the flowers; I couldn't carry both.\nGone to the cistern. Don't follow me over the steps.",
            "IONA\nShe wrapped the handle in sailcloth.\nStill worried I'll skin my knuckles."
        };
        public static readonly string[] Pump={
            "MARA'S PUMP LOG\nDrain open. Waves still cross the causeway.\nWatch the spray: when the warning boards flash,\nget into the eastern recess or off the path."
        };
        public static readonly string[] Lens={
            "IONA\nNo cracks. She used her coat to wrap it.\nMara, you'd better have found somewhere dry."
        };
        public static readonly string[] Relay={
            "MAINTENANCE PLATE\nLens in the western socket. Service receiver first;\nit releases the door to the last mirror.\nThen send the beam to the stair receiver.",
            "IONA\nThe door latch holds once it's lit.\nI can turn the light away and keep going."
        };
        public static readonly string[] Ending={
            "ORRIN, CALLING UP FROM THE LANDING\nThat's enough light! I'm taking the boat round.\nThere's a lamp moving under the cistern.",
            "IONA\nThree short flashes. Mara's workshop signal.\nThe lower beacon can run without me now."
        };
        public static string Objective(Journey j) {
            if(j.Has(Restoration.Watch))return "Chapter two complete : observatory next";
            if(j.Has(Restoration.Mara))return "Take the dry western passage with Mara";
            if(j.Has(Restoration.Rescue))return "Reach Mara behind the eastern shutter";
            if(j.Has(Restoration.CaveGate))return "Use the fixed mirrors to light Mara's door receiver";
            if(j.Has(Restoration.Cistern))return j.CaveCheckpoint>0?"Open the cave gallery with your focused lantern":"Take the eastern stairs into the sea caves";
            if(j.Has(Restoration.Drain))return "Send light through the fixed mirrors to release the bridge";
            if(j.Has(Restoration.Focus))return "Light the drain receiver to open the inspection walk";
            if(j.Has(Restoration.Beacon))return "Find Mara's lantern sleeve in the cistern workshop";
            if(!j.Has(Restoration.Harbor))return "Turn the harbor mirror to power the bridge";
            if(!j.Has(Restoration.Wrench))return "Find Mara's wrench in the eastern grove";
            if(!j.Has(Restoration.Pump))return "Free the drain at the north-west pump house";
            if(!j.Has(Restoration.Lens))return "Cross the tide causeway for the lens";
            if(!j.Has(Restoration.Mounted))return "Fit the lens in the court's western socket";
            if(!j.Has(Restoration.CourtUnlocked))return "Light the service receiver to open the mirror room";
            if(!j.Has(Restoration.Relay))return "Turn the route toward the stair receiver";
            return "Start the lower beacon at the lighthouse stairs";
        }
        public static string[] Journal(Journey j)=>new[]{
            "BREAKWATER NOTES\n"+Objective(j)+".\n\nMove with WASD or arrows. E works nearby machinery.\nEscape pauses. Q closes these notes.",
            "ROUTES\nGrove east of the bridge. Pump house north-west.\nMirror court beyond the grove; lighthouse east of it.\nThe eastern pump recess stays above the waves.",
            j.Has(Restoration.Archive)?"FATHER'S REPAIR BOOK\nHe replaced the pump rail twice and kept the bent\nbolts. Mara's handwriting: 'Replace the steps next.'\nHe hasn't crossed it out.":"MARA\nHer tools and notes are still here.\nOrrin saw her take the cliff path."
        };
    }
}
