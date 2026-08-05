# SINGLE EFFECT AND MULTI EFFECT UNIFICATION

The purpose of this branch is to unify the functionality of the single effect and multi effect concepts and ultimately collapse them down to a single concept absorbing the functinoality of multi effects.

## INITIAL THOUGHTS TO BE REVIEWED

I need to rethink my approach for how 'Single Fixture' and 'Multi-Fixture' effects are applied. There really should not be a distinction between the two. 

Regardless of how many fixtures are in a 'voice', the effects should be able to dynamically handle apply to them. There will rarely be a case where an effect does not generatively/procedurally apply across all of the defined fixtures. 

I was ideating around a concept of fixture 'groups' and this could imply a collection of fixtures, or even a long LED strip (being a collection of RGB pixels), or a collection of LED strips etc. This way, when binding a control to a fixture + effect, selection becomes more about what parts of the 'orchestra' this will apply to.

Some key things to consider and audit:

* fixture groups are what get bound to a control and effect
* effects should ALWAYS apply across however many sources are in a group. 
    * For example, a single fixture with 6 color sources, in a group of 3 identical fixtures, will have 18 color sources in that group. An effect (like a gradient effect) will dynamically apply across all of those sources.
    * If that same fixture has only a single dimmer source, than a group of 3 identical fixtures would have 3 dimmer sources that an effect would apply to (such as a noise or chase).
    * If an effect applies to both a Color source and a Dimmer source (like a Noise effect), than for this group of fixtures defined above, the Dimmer source being a Float parameter will be dynamically applied with a noise value for each of the 3 dimmer sourecs, and the Color source being a Color RGB parameter will be dynamically applied a Color Noise value to each of the 18 Color sources.
* This will scale to any sized group with any amount of sources.
    * for example, an LED strip would be some number of RGB sources representing pixels. This could be 100 Color sources. an effect will be able to handle this.
* if a control and effect are bound to multiple groups, than the effect will apply to each group accordingly.
    * at some point there will need to be a consideration about whether groups are mapped 'end-to-end' or if each group is treated as its own application of an effect.

## CHANGES TO CURRENT EFFECT

There will need to be some UI/UX changes to accomodate/properly illustrate the state of current, sources, and modulation. Noise Modulation shouldn't be 'per source' it should just be a gradient/strip across all sources. Same thing with most represenations of value that isn't a curve. You can think about it like communicating to the user 'each source will sample across this strip of gradient (float or rgb)' 

## WHAT I NEED

I would like to audit this concept, come up with an approach of how something like this can be implemented in NAP (regardless of it being a refactor), and then what parts of the architecture specifically need to be refactored and how that would work within the context of our application.
