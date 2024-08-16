/// <reference types="node" />

/**
 * Generate ssdeep hash asynchronously.
 * @param contents The data to hash.
 * @returns A promise to be resolved with the resulting hash.
 * @throws {TypeError} When the input is malformed.
 */
export declare function hash(contents: string | Buffer) : Promise<string>;

/**
 * Generate ssdeep hash synchronously.
 * @param contents The data to hash.
 * @returns The resulting hash.
 * @throws {TypeError} When the input is malformed.
 */
export declare function hashSync(contents: string | Buffer) : string;

/**
 * Compare ssdeep hashes asynchronously.
 * @param hash1 The first hash to compare.
 * @param hash2 The second hash to compare.
 * @returns A promise to be resolved with the similarity score for the input hashes, between 0-100.
 * @throws {TypeError} When the hashes are malformed.
 */
export declare function compare(hash1: string, hash2: string): Promise<number>;

/**
 * Compare ssdeep hashes synchronously.
 * @param hash1 The first hash to compare.
 * @param hash2 The second hash to compare.
 * @returns The similarity score for the input hashes, between 0-100.
 * @throws {TypeError} When the hashes are malformed.
 */
export declare function compareSync(hash1: string, hash2: string): number;
