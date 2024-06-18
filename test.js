//TODO: add these tests https://github.com/DinoTools/python-ssdeep/blob/master/tests/test_lib.py

console.log("starting");

const ssdeep = require("./build/Release/fast-ssdeep.node");

console.log("required");

console.log(ssdeep); //TODO: why is this empty?
console.log(ssdeep.hash); //TODO: why is this anonymous?

const promise = ssdeep.hash("Hello world!");
console.log("promise", promise);

promise.then(res => console.log("result", res)).catch(err => console.log("error", err));

(async () => {
    const hash1 = await ssdeep.hash("X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*");
    console.log("hash1", hash1);

    const hash2 = await ssdeep.hash("X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-THREATPINCH-ANTIVIRUS-TEST-FILE!$H+H*");
    console.log("hash2", hash2);

    console.log("compared", await ssdeep.compare(hash1, hash2));

    console.log("compareSync", ssdeep.compareSync(hash1, hash2));
})().catch(err => console.log("bruh err", err));

console.log("hashSync", ssdeep.hashSync("Hello world!"));