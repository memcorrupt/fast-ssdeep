# fast-ssdeep

[![Version](https://img.shields.io/npm/v/fast-ssdeep.svg)](https://www.npmjs.org/package/fast-ssdeep)
[![Downloads](https://img.shields.io/npm/dm/fast-ssdeep.svg)](https://www.npmjs.com/package/fast-ssdeep)
[![Build Status](https://github.com/memcorrupt/fast-ssdeep/actions/workflows/build.yml/badge.svg)](https://github.com/memcorrupt/fast-ssdeep/actions/workflows/build.yml)
[![License](https://img.shields.io/github/license/memcorrupt/fast-ssdeep)](LICENSE)

Node.js binding for the ssdeep CTPH library.

[ssdeep project website](https://ssdeep-project.github.io/ssdeep/index.html)

# Installation

Install the [fast-ssdeep](https://npmjs.org/package/fast-ssdeep) package from the NPM registry using your package manager of choice.

Example:

```
npm install fast-ssdeep
# or
yarn add fast-ssdeep
# or
pnpm install fast-ssdeep
```

# Documentation

### `ssdeep.hash(contents: string|Buffer): Promise<string>`

Asynchronously calculate an ssdeep hash based on the `contents` argument, which can be a string or Buffer.

### `ssdeep.hashSync(contents: string|Buffer): string`

Synchronously calculate an ssdeep hash based on the `contents` argument, which can be a string or Buffer.

### `ssdeep.compare(hash1: string, hash2: string): Promise<number>`

Asynchronously compare two ssdeep hashes to generate a similarity score. Both hashes must be passed as strings, and a similarity between 0-100 will be returned.

### `ssdeep.compareSync(hash1: string, hash2: string): number`

Synchronously compare two ssdeep hashes to generate a similarity score. Both hashes must be passed as strings, and a similarity between 0-100 will be returned.