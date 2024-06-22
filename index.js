const ssdeep = require("./build/Release/fast-ssdeep.node");

module.exports = {
  /**
   * Generate hash
   * @param {string} input
   * @returns {string}
   * @throws {TypeError} When input is not a string
   */
  hashSync: input => ssdeep.hashSync(input),

  /**
   * Generate hash asynchronously
   * @param {string} input
   * @returns {string}
   * @throws {TypeError} When input is not a string
   */
  hash: input => ssdeep.hash(input),

  /**
   * Compare two hashes
   * @param {string} hash1
   * @param {string} hash2
   * @returns {number} Comparison score
   * @throws {TypeError} When either of the arguments is not a string or hashes malformed
   */
  compareSync: (hash1, hash2) => ssdeep.compareSync(hash1, hash2),

  /**
   * Compare two hashes asynchronously
   * @param {string} hash1
   * @param {string} hash2
   * @returns {number} Comparison score
   * @throws {TypeError} When either of the arguments is not a string or hashes malformed
   */
  compare: (hash1, hash2) => ssdeep.compare(hash1, hash2),
};
