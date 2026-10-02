import { db } from '../../common/db.js';
import { getCached, setCached, invalidateCache } from '../../common/redis.js';
import { NotFoundError } from '../../common/errors.js';

export class GameService {
  async listGames() {
    const cacheKey = 'games:all';
    const cached = await getCached(cacheKey);
    if (cached) return cached;

    const games = await db.game.findMany({
      include: {
        profiles: {
          where: { isOfficial: true },
          take: 1,
        },
      },
      orderBy: { title: 'asc' },
    });

    await setCached(cacheKey, games, 600);
    return games;
  }

  async getGameBySlug(slug: string) {
    const cacheKey = `game:slug:${slug}`;
    const cached = await getCached(cacheKey);
    if (cached) return cached;

    const game = await db.game.findUnique({
      where: { slug },
      include: {
        profiles: {
          orderBy: { downloads: 'desc' },
        },
      },
    });

    if (!game) {
      throw new NotFoundError(`Game '${slug}' not found`);
    }

    await setCached(cacheKey, game, 600);
    return game;
  }

  async detectGameByExecutable(exeName: string) {
    const normalizedExe = exeName.toLowerCase();
    const cacheKey = `game:exe:${normalizedExe}`;
    const cached = await getCached(cacheKey);
    if (cached) return cached;

    const games = await db.game.findMany();
    const match = games.find((g: any) =>
      g.exeNames.some((e: any) => e.toLowerCase() === normalizedExe)
    );

    if (!match) {
      return null;
    }

    await setCached(cacheKey, match, 1800);
    return match;
  }

  async upsertGame(data: {
    title: string;
    slug: string;
    exeNames: string[];
    graphicsApis: string[];
    antiCheat?: string;
    supported?: boolean;
  }) {
    const game = await db.game.upsert({
      where: { slug: data.slug },
      update: data,
      create: data,
    });

    await invalidateCache('games:*');
    await invalidateCache(`game:slug:${data.slug}`);
    return game;
  }
}
