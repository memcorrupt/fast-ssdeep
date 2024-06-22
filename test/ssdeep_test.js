const assert = require("assert");
const { describe, it } = require("mocha");
const ssdeep = require("../build/Release/fast-ssdeep.node"); //TODO!

describe("should generate correct hashes", function(){
    const hashes = {
        ["Hello world!"]: "3:agPE:agPE",
        ["X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*"]: "3:a+JraNvsgzsVqSwHq9:tJuOgzsko",
        ["Also called fuzzy hashes, Ctph can match inputs that have homologies."]: "3:AXGBicFlgVNhBGcL6wCrFQEv:AXGHsNhxLsr2C"
    };

    describe("synchronous", function(){
        for(const [text, hash] of Object.entries(hashes)){
            it(text, async () => {
                const actualHash = ssdeep.hashSync(text);
                assert.strictEqual(hash, actualHash);
            });
        }
    });

    describe("async promises", function(){
        for(const [text, hash] of Object.entries(hashes)){
            it(text, async () => {
                const actualHash = await ssdeep.hash(text);
                assert.strictEqual(hash, actualHash);
            });
        }
    });
});

describe("should return proper similarity values", function(){
    const comparisons = [
        ["3:AXGBicFlgVNhBGcL6wCrFQEv:AXGHsNhxLsr2C", "3:AXGBicFlIHBGcL6wCrFQEv:AXGH6xLsr2C", 22],
        ["3:a+JraNvsgzsVqSwHq9:tJuOgzsko", "3:a+JraNvsg7QhyqzWwHq9:tJuOg7Q4Wo", 18]
    ];

    describe("synchronous", function(){
        for(const [hash1, hash2, score] of comparisons){
            it(`${hash1} vs ${hash2}`, async () => {
                const actualScore = ssdeep.compareSync(hash1, hash2);
                assert.strictEqual(score, actualScore);
            })
        }
    });

    describe("async promises", function(){
        for(const [hash1, hash2, score] of comparisons){
            it(`${hash1} vs ${hash2}`, async () => {
                const actualScore = await ssdeep.compare(hash1, hash2);
                assert.strictEqual(score, actualScore);
            })
        }
    });
});

/*describe("should reject malformed input", function(){
    it("TODO!"); //TODO!
});*/