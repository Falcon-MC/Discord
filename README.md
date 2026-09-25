<p align="center">
	<picture>
		<source media="(prefers-color-scheme: dark)" srcset="https://raw.githubusercontent.com/Falcon-MC/Falcon/main/.github/logo-white.png">
		<img src="https://raw.githubusercontent.com/Falcon-MC/Falcon/main/.github/logo.png" alt="Falcon" width="200">
	</picture>
	<br>
	<b>Falcon Discord</b>
	<br>
	The bot of the Falcon Discord server
</p>

<p align="center">
	<a href="https://github.com/Falcon-MC/Discord/actions/workflows/ci.yml"><img src="https://github.com/Falcon-MC/Discord/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
	<img src="https://img.shields.io/badge/language-C%2B%2B20-00599C" alt="C++20">
	<img src="https://img.shields.io/badge/library-D%2B%2B-5865F2" alt="D++">
	<img src="https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20Docker-lightgrey" alt="Platform">
</p>

## What is this?

The bot running on the [Falcon](https://github.com/Falcon-MC/Falcon) Discord server, written in C++20 with
[D++](https://github.com/brainboxdotcc/DPP).

- **`/contributor`** - gives the Contributor role to anyone with a merged pull request or a commit in a public
  repository of the organization. The member links their GitHub account with a one time code, and the bot only
  reads their public profile. Members who already have the role cannot run it.
- **`/embed`** - administrators only. Sends an embed with a title, a description (`\n` for a new line) and an
  optional hex color in the current channel.

## Setup

1. Create an application in the [Discord Developer Portal](https://discord.com/developers/applications), copy
   the bot token and invite the bot with the `bot` and `applications.commands` scopes and the **Manage Roles**,
   **Send Messages** and **Embed Links** permissions. Its role must be above the Contributor role.
2. Create an OAuth app in the organization settings on GitHub (**Developer settings** → **OAuth Apps**), tick
   **Enable Device Flow** and copy its client ID. The callback URL is not used, any URL works.
3. Set the environment variables, listed in [`.env.example`](.env.example):

| Variable | Description |
|---|---|
| `DISCORD_TOKEN` | Bot token |
| `DISCORD_GUILD_ID` | ID of the server the commands are registered in |
| `DISCORD_CONTRIBUTOR_ROLE_ID` | ID of the Contributor role |
| `GITHUB_CLIENT_ID` | Client ID of the GitHub OAuth app |
| `GITHUB_ORGANIZATION` | Organization whose repositories count, `Falcon-MC` by default |

## Running

With Docker:

```
docker build -t falcon-discord .
docker run -d --restart unless-stopped --env-file .env falcon-discord
```

Or from a build, with the variables set in the environment:

```
./build/FalconDiscord
```

## Building

Requires CMake 3.21+, a C++20 compiler, OpenSSL and zlib. On Windows, D++ ships its own OpenSSL and zlib and
they are copied next to the executable. The first configure needs network access to fetch D++.

```
cmake -B build -G Ninja
cmake --build build
```

## Related repositories

- [Falcon](https://github.com/Falcon-MC/Falcon) - the server
- [PluginAPI](https://github.com/Falcon-MC/PluginAPI) - plugin API
- [falcon-mc.github.io](https://github.com/Falcon-MC/falcon-mc.github.io) - website

## Licensing information

Falcon Discord is licensed under the [GNU Lesser General Public License v3.0](LICENSE), which supplements the
[GNU General Public License v3.0](COPYING).

Falcon is not affiliated with Mojang or Discord. All brands and trademarks belong to their respective owners.
