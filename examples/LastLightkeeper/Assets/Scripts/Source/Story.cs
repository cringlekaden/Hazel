namespace LastLightkeeper {
    public static class Story {
        public static readonly string[] Arrival={
            "ORRIN\nBridge won't lower. I tried the wheel.\nCan you turn that mirror? The motor takes light\nfrom the harbor lamp.",
            "IONA\nThe cover's off. Father was working on it.\nKeep the boat tied up; I'll check the latch."
        };
        public static readonly string[] Ferryman={
            "ORRIN\nMy sister's on the survey ledge. I saw her waving.\nI know this reef in daylight, but I can't risk\nthe boat without the beacon.",
            "ORRIN\nMara took the cliff path before the steps fell.\nShe left her wrench in the grove. Yellow handle."
        };
        public static readonly string[] Grove={
            "MARA'S NOTE\nLens on the pump platform. Drain wheel seized.\nWrench by the flowers; couldn't carry both.\nI'll try the cistern path. Stay off the steps."
        };
        public static readonly string[] Pump={
            "TIDE NOTICE\nThe drain clears the platform, not the waves.\nFlashing boards mean a surge is coming.\nWait in the eastern recess until it passes."
        };
        public static readonly string[] Lens={
            "IONA\nNot a scratch. She wrapped it in her coat.\nI hope she kept something for herself."
        };
        public static readonly string[] Relay={
            "COURT MAINTENANCE\nFit the lens in the western socket.\nLight the service receiver to release the mirror\nroom. Then light the receiver by the stairs.",
            "IONA\nThese latches hold after the beam moves.\nOne door at a time."
        };
        public static readonly string[] Ending={
            "ORRIN, BELOW\nI can see the reef! I'm going round for them.\nThere's another lamp under the cistern.",
            "IONA\nThree short flashes. That's Mara.\nThe beacon's steady. I can leave it running."
        };
        public static string Objective(Journey j) {
            if(j.Has(Restoration.Home))return "The main light is steady. Revisit the landing or take the boat.";
            if(j.Has(Restoration.TowerBeacon))return "Meet everyone at the landing; take the morning boat when ready";
            if(j.Has(Restoration.TowerDrive))return "Start the main light in the northern lantern bay";
            if(j.Has(Restoration.TowerEntry))return "Latch the north, east and south drive contacts - any order";
            if(j.Has(Restoration.Observatory))return "The survey crew is safe. The upper tower route is ready.";
            if(j.Has(Restoration.Winch))return "Meet the crew at the western tower stairs";
            if(j.Has(Restoration.Crew))return "Light the tower winch from the northern survey post";
            if(j.Has(Restoration.Dome))return "Find Nessa and Cal in the northern instrument room";
            if(j.Has(Restoration.Span))return "Clear the carriage; light the west latch from the east terrace";
            if(j.Has(Restoration.Collimator))return "Light the span latch across the collapsed survey hall";
            if(j.Has(Restoration.Watch))return "Take Mara's ledger up to the observatory";
            if(j.Has(Restoration.Mara))return "Take the dry western passage with Mara";
            if(j.Has(Restoration.Rescue))return "Go through the eastern shutter to reach Mara";
            if(j.Has(Restoration.CaveGate))return "Light Mara's door receiver through the fixed mirrors";
            if(j.Has(Restoration.Cistern))return j.CaveCheckpoint>0?"Light the gallery receiver beyond the first shutter":"Take the eastern stairs into the sea caves";
            if(j.Has(Restoration.Drain))return "Light the bridge receiver from the raised walk";
            if(j.Has(Restoration.Focus))return "Light the drain receiver through the shutter";
            if(j.Has(Restoration.Beacon))return "Find the lantern sleeve on Mara's workshop bench";
            if(!j.Has(Restoration.Harbor))return "Turn the harbor mirror to lower the bridge";
            if(!j.Has(Restoration.Wrench))return "Find Mara's wrench in the eastern grove";
            if(!j.Has(Restoration.Pump))return "Free the drain wheel at the north-west pump house";
            if(!j.Has(Restoration.Lens))return "Cross the tide causeway for the lens";
            if(!j.Has(Restoration.Mounted))return "Fit the lens in the court's western socket";
            if(!j.Has(Restoration.CourtUnlocked))return "Light the service receiver to open the mirror room";
            if(!j.Has(Restoration.Relay))return "Send the beam to the receiver by the stairs";
            return "Start the lower beacon at the lighthouse stairs";
        }
        public static string[] Journal(Journey j)=>new[]{
            "BREAKWATER\n"+Objective(j)+".\n\nWASD or arrows: move. E: use nearby equipment.\nEsc: pause. Q: close these notes.",
            "ROUTES\nGrove east of the bridge. Pump house north-west.\nMirror court north of the grove; lighthouse east.\nThe pump's eastern recess stays above the waves.",
            j.Has(Restoration.Archive)?"FATHER'S REPAIR BOOK\nPump rail replaced twice. Bent bolts kept for spares.\nMara added: 'Replace the steps next.'\nHe hasn't crossed that out.":"MARA\nOrrin saw her take the cliff path.\nShe left her wrench in the grove."
        };
    }
}
