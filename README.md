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
- **Anti-spam** - a member who sends 6 messages in 6 seconds, the same message 3 times in 30 seconds or a
  message with 5 mentions or more has those messages deleted and is timed out for 10 minutes.
- **Anti-invite** - invites to other Discord servers are deleted, including in edited messages. Invites to this
  server are allowed.

Members who can manage messages are never moderated automatically.

### Moderation commands

| Command | Permission | Description |
|---|---|---|
| `/ban user [reason] [delete_days]` | Ban Members | Bans a user, optionally deleting up to 7 days of their messages |
| `/tempban user duration [reason]` | Ban Members | Bans a user for a duration such as `12h`, `7d` or `1w2d`, up to one year |
| `/unban user [reason]` | Ban Members | Lifts a ban, the user can be given by ID |
| `/kick user [reason]` | Kick Members | Kicks a member |
| `/mute user duration [reason]` | Moderate Members | Times out a member, up to 28 days |
| `/unmute user [reason]` | Moderate Members | Removes a timeout |
| `/warn user reason` | Moderate Members | Warns a member |
| `/warnings user` | Moderate Members | Lists the active warnings of a user |
| `/history user` | Moderate Members | Lists every case of a user |
| `/clearwarnings user` | Administrator | Clears the active warnings of a user |
| `/clear amount [user]` | Manage Messages | Deletes up to 100 recent messages, optionally from one user |

Commands are hidden from members without the permission, and the bot checks it again when they run. A moderator
cannot act on themselves, the bot, the server owner or a member whose highest role is not below theirs, and the
bot must also be above the target. Members are sent a direct message before a ban or kick and after a mute or
warning.

Every action gets a case number and is stored in `moderation.json` in the data directory. Tempbans are lifted
automatically, checked every minute and at startup. Cases and automatic actions are posted in the moderation log
channel when one is set.

## Setup

1. Create an application in the [Discord Developer Portal](https://discord.com/developers/applications), enable
   the **Message Content** intent, copy the bot token and invite the bot with the `bot` and
   `applications.commands` scopes and the **Manage Roles**, **Manage Messages**, **Read Message History**,
   **Moderate Members**, **Kick Members**, **Ban Members**, **Send Messages** and **Embed Links** permissions. Its
   role must be above the Contributor role and the roles of the members it moderates.
2. Create an OAuth app in the organization settings on GitHub (**Developer settings** → **OAuth Apps**), tick
   **Enable Device Flow** and copy its client ID. The callback URL is not used, any URL works.
3. Set the environment variables, listed in [`.env.example`](.env.example):

| Variable | Description |
|---|---|
| `DISCORD_TOKEN` | Bot token |
| `DISCORD_GUILD_ID` | ID of the server the commands are registered in |
| `DISCORD_CONTRIBUTOR_ROLE_ID` | ID of the Contributor role |
| `DISCORD_MOD_LOG_CHANNEL_ID` | Optional, ID of the channel moderation actions are logged in |
| `GITHUB_CLIENT_ID` | Client ID of the GitHub OAuth app |
| `GITHUB_ORGANIZATION` | Organization whose repositories count, `Falcon-MC` by default |
| `DATA_DIRECTORY` | Where `moderation.json` is stored, `data` by default |

## Running

With Docker:

```
docker build -t falcon-discord .
docker run -d --restart unless-stopped --env-file .env -v falcon-discord-data:/app/data falcon-discord
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
