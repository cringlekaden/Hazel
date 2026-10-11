namespace LastLightkeeper {
    // Authored dialogue and journal are content, not a reusable dialogue engine.
    public static class Story {
        public static readonly string[] Arrival={
            "IONA / The keeper\nThree nights without a light. The sea has started\nreturning things we buried long ago.",
            "ORRIN / The ferryman\nI can bring help if you wake the lower signal.\nYour father's harbor mirror is still standing.",
            "ORRIN\nTurn it toward the receiver by the bridge.\nThe beam will show you where it wants to go."
        };
        public static readonly string[] Ferryman={
            "ORRIN\nI watched your father polish that mirror every morning.\nHe said a keeper never lets someone sail alone.",
            "ORRIN\nThe pump house is inland, past the bridge. Mara's\nwrench washed into the grove east of here.",
            "ORRIN\nBring the lens back from the tide works. I'll stay\nwith the boat until there's a light to follow."
        };
        public static readonly string[] Grove={
            "MARA'S FIELD NOTE\nThe island's three relays share one heartbeat.\nIf the harbor still lives, the tower can live too.",
            "MARA'S FIELD NOTE\nA mirror turns east into north, or east into south.\nRead the beam before you turn the next mirror.",
            "IONA\nShe left the wrench where the wildflowers grow.\nEven in a storm, Mara thinks of small kindnesses."
        };
        public static readonly string[] Pump={
            "PUMP LOG / Mara\nThe sluice wheel has seized. Use my wrench to\nopen the lower drain; the lens is on the far platform.",
            "PUMP LOG\nThe sea still surges across the causeway. Blue means\nclear. Amber warns. Red means find higher ground.",
            "PUMP LOG\nUse the eastern alcove if you cannot reach the far\nplatform. A wave is faster than a keeper."
        };
        public static readonly string[] Lens={
            "IONA\nThe lens is intact. A little salt, a little stubbornness.\nI used to think that was all this island was made of.",
            "IONA\nBut someone carried it here instead of saving\ntheir own things. Mara is still out there."
        };
        public static readonly string[] Relay={
            "RELAY INSTRUCTIONS\nFit the lens in the western socket. Turn each mirror\nuntil the light reaches the eastern receiver.",
            "RELAY INSTRUCTIONS\nStone blocks light. Mirrors bend it. An unlit\nreceiver means the route is still incomplete."
        };
        public static readonly string[] Ending={
            "IONA\nThe old mechanism catches. For a moment the island\nholds its breath. Then the lower lantern wakes.",
            "ORRIN / From the harbor\nI see it! Keep that light burning, Iona.\nI'm bringing someone home.",
            "IONA\nBeyond the cliffs, another signal answers.\nMara. The upper tower can wait until morning."
        };
        public static string Objective(Journey j) {
            if(!j.Has(Restoration.Harbor))return "Wake the harbor receiver / turn the mirror";
            if(!j.Has(Restoration.Wrench))return "Find Mara's wrench / east in the salt grove";
            if(!j.Has(Restoration.Pump))return "Repair the sluice / north-west pump house";
            if(!j.Has(Restoration.Lens))return "Recover the lens / cross the tide causeway";
            if(!j.Has(Restoration.Mounted))return "Fit the lens / western relay socket";
            if(!j.Has(Restoration.Relay))return "Route the light / three mirrors, one receiver";
            return "Ignite the lower beacon / lighthouse stairs";
        }
        public static string[] Journal(Journey j)=>new[]{
            "KEEPER'S JOURNAL / Restoration\n"+Objective(j)+".\nWASD / arrows move. E interacts. Q closes this book.",
            "THE BREAKWATER / Island paths\nHarbor south-west. Salt grove east of the bridge.\nPump house north-west. Mirror court north-east.",
            "MY FATHER'S RULE\nFollow the light. If it meets stone, change its path.\nIf it meets someone, let it guide them home.",
            j.Has(Restoration.Archive)?"A LETTER NEVER SENT\nFather kept a place at the table for Mara.\nHe knew she would come back. Now I know it too.":"A BLANK PAGE\nThere is a keeper's archive hidden beside the grove.\nSome repairs begin with remembering."
        };
    }
}
