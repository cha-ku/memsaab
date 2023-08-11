# memsaab

[![ci](https://github.com/cha-ku/memsaab/actions/workflows/ci.yml/badge.svg)](https://github.com/cha-ku/memsaab/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/cha-ku/memsaab/branch/main/graph/badge.svg)](https://codecov.io/gh/cha-ku/memsaab)
[![CodeQL](https://github.com/cha-ku/memsaab/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/cha-ku/memsaab/actions/workflows/codeql-analysis.yml)

## About memsaab
* Simple memcached implementation, supports basic memcached commands - `set`, `get`, `append`, `prepend`, `add`, `replace`
* Supports key expiry
* Can connect to multiple clients at once (uses asynchronous I/O operations with the help of
[libuv](https://github.com/libuv/libuv))

## Example Usage

* `set` and `get`
```bash
> telnet 127.0.0.1 11211
Trying 127.0.0.1...
Connected to 127.0.0.1.
Escape character is '^]'.
set test 1 0 7 #key: test, flags: 1, expiry: 0 (never), byte_count of corresponding value: 7
testing
STORED
get test
VALUE test 1 7
testing
END
```

* `set` key with expiry time to 100 seconds
```bash
set test2 0 100 4 noreply #expiry: current_time() + 100 seconds, noreply: true
testing2
get test2
VALUE test2 0 4
test
END
```

* `get` expired key. Memcached uses lazy removal meaning an expired key is only removed when it is queried

```bash
set wxyz 0 10 3 #[2023-08-11 09:56:59.224] [info] Expiry time set to 2023-08-11 09:57:09
abcd
STORED
get wxyz #[2023-08-11 09:57:19.272] [info] Item expired on 2023-08-11 09:57:09, current time: 2023-08-11 09:57:19
END
```

Inspired by [Coding Challenges #17](https://codingchallenges.substack.com/p/coding-challenge-17-memcached-server)