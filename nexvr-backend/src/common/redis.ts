import Redis from 'ioredis';
import { config } from './config.js';
import { logger } from './logger.js';

export const redis = new Redis(config.REDIS_URL, {
  lazyConnect: true,
  maxRetriesPerRequest: 3,
  retryStrategy(times) {
    const delay = Math.min(times * 100, 3000);
    return delay;
  },
});

redis.on('connect', () => {
  logger.info('Connected to Redis cache server');
});

redis.on('error', (err) => {
  logger.warn({ err: err.message }, 'Redis connection error; operating in degraded mode');
});

export async function getCached<T>(key: string): Promise<T | null> {
  try {
    const data = await redis.get(key);
    return data ? (JSON.parse(data) as T) : null;
  } catch {
    return null;
  }
}

export async function setCached<T>(key: string, value: T, ttlSeconds = 300): Promise<void> {
  try {
    await redis.set(key, JSON.stringify(value), 'EX', ttlSeconds);
  } catch (err) {
    logger.warn({ err, key }, 'Failed to set cache key');
  }
}

export async function invalidateCache(keyOrPattern: string): Promise<void> {
  try {
    if (keyOrPattern.includes('*')) {
      const keys = await redis.keys(keyOrPattern);
      if (keys.length > 0) {
        await redis.del(...keys);
      }
    } else {
      await redis.del(keyOrPattern);
    }
  } catch (err) {
    logger.warn({ err, keyOrPattern }, 'Failed to invalidate cache');
  }
}
