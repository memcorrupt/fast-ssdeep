const ssdeep = require("./build/Release/fast-ssdeep.node");

module.exports = {
  /**
   * Generate hash
   * @param {string} input
   * @returns {string}
   */
  hashSync: input => ssdeep.hashSync(input),

  /**
   * Generate hash asynchronously
   * @param {string} input
   * @returns {string}
   */
  hash: input => ssdeep.hash(input),

  /**
   * Compare two hashes
   * @param {string} hash1
   * @param {string} hash2
   * @returns {number} Comparison score
   */
  compareSync: (hash1, hash2) => ssdeep.compareSync(hash1, hash2),

  /**
   * Compare two hashes asynchronously
   * @param {string} hash1
   * @param {string} hash2
   * @returns {number} Comparison score
   */
  compare: (hash1, hash2) => ssdeep.compare(hash1, hash2),
};
