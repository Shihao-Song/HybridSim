## I collected **10M instructions for every 250M instructions**. My goal is to find portions that have most memory interactions (huge number of LLC evictions and loads).

### Workloads that generate **at least 1 memory access for every 1k instructions**
+ AI:
    + 531.deepsjeng\_r
+ Scientific Domain:
    + 503.bwaves\_r
    + 507.cactuBSSN\_r
    + 510.parest\_r
    + 519.lbm\_r
    + 521.wrf\_r
    + 527.cam4\_r
    + 554.roms\_r
+ Simulation:
    + 520.omnetpp\_r
+ Visualization:
    + 526.blender\_r

### Possible Domain Apps (Combinations of applications in the same domain):
+ AI:
    + 531.deepsjeng\_r \+ 541.leela\_r
    + 505.mcf\_r \+ 531.deepsjeng\_r
    + 531.deepsjeng\_r \+ 548.exchange2\_r
    + 505.mcf\_r \+ 531.deepsjeng\_r \+ 548.exchange2\_r \+ 541.leela\_r
+ Scientific:
    + 507.cactuBSSN\_r \+ 519.lbm\_r
    + 503.bwaves\_r \+ 507.cactuBSSN\_r
    + 521.wrf\_r \+ 527.cam4\_r
    + 527.cam4\_r \+ 554.roms\_r
+ AI\+Visualization:
    + 526.blender\_r \+ 531.deepsjeng\_r
    + 511.povray\_r \+ 531.deepsjeng\_r
+ AI\+Scientific:
    + 527.cam4\_r \+ 531.deepsjeng\_r
    + 531.deepsjeng\_r \+ 554.roms\_r

### Random Combinations
