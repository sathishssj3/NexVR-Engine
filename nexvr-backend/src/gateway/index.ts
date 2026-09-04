import express from 'express';
import helmet from 'helmet';
import cors from 'cors';
import morgan from 'morgan';
import rateLimit from 'express-rate-limit';
import { config } from '../common/config.js';
import { logger } from '../common/logger.js';
import { errorHandler, NotFoundError } from '../common/errors.js';
import { db } from '../common/db.js';
import { redis } from '../common/redis.js';

// Microservice Route Imports
import { authRouter } from '../services/auth/auth.routes.js';
import { userRouter } from '../services/user/user.routes.js';
import { gameRouter } from '../services/game/game.routes.js';
import { profileRouter } from '../services/profile/profile.routes.js';
import { updateRouter } from '../services/update/update.routes.js';
import { telemetryRouter } from '../services/telemetry/telemetry.routes.js';

const app = express();

// Security Headers
app.use(helmet());

// CORS Policy
const allowedOrigins = config.CORS_ORIGINS.split(',').map((s) => s.trim());
app.use(
  cors({
    origin: (origin, callback) => {
      // Allow requests with no origin (like mobile apps, Electron, or curl)
      if (!origin || allowedOrigins.includes(origin)) {
        callback(null, true);
      } else {
        callback(new Error('Blocked by CORS policy'));
      }
    },
    credentials: true,
  })
);

// Body Parsers
app.use(express.json({ limit: '10mb' }));
app.use(express.urlencoded({ extended: true }));

// HTTP Request Logging
app.use(
  morgan('combined', {
    stream: {
      write: (message: string) => logger.info(message.trim()),
    },
  })
);

// Global Rate Limiting
const limiter = rateLimit({
  windowMs: config.RATE_LIMIT_WINDOW_MS,
  max: config.RATE_LIMIT_MAX,
  standardHeaders: true,
  legacyHeaders: false,
  message: { status: 'error', message: 'Too many requests, please try again later.' },
});
app.use('/api/', limiter);

// Liveness & Readiness Probes
app.get('/health', async (_req, res) => {
  let dbOk = false;
  let redisOk = false;

  try {
    await db.$queryRaw`SELECT 1`;
    dbOk = true;
  } catch {
    dbOk = false;
  }

  try {
    redisOk = redis.status === 'ready' || redis.status === 'connect';
  } catch {
    redisOk = false;
  }

  const isHealthy = dbOk; // Redis failure degrades caching but does not crash app
  res.status(isHealthy ? 200 : 503).json({
    status: isHealthy ? 'healthy' : 'unhealthy',
    timestamp: new Date().toISOString(),
    uptime: process.uptime(),
    components: {
      database: dbOk ? 'up' : 'down',
      cache: redisOk ? 'up' : 'down',
    },
  });
});

// Mount Version 1 APIs
app.use('/api/v1/auth', authRouter);
app.use('/api/v1/user', userRouter);
app.use('/api/v1/games', gameRouter);
app.use('/api/v1/profiles', profileRouter);
app.use('/api/v1/update', updateRouter);
app.use('/api/v1/telemetry', telemetryRouter);

// 404 Route Handler
app.use((_req, _res, next) => {
  next(new NotFoundError('Requested API endpoint does not exist'));
});

// Centralized Error Handling Middleware
app.use(errorHandler);

// Start Gateway Server
const server = app.listen(config.PORT, () => {
  logger.info(
    { port: config.PORT, env: config.NODE_ENV },
    `NexVR API Gateway running at http://localhost:${config.PORT}`
  );
});

// Graceful Termination
function handleShutdown(signal: string) {
  logger.info(`Received ${signal}. Gracefully shutting down...`);
  server.close(async () => {
    logger.info('HTTP server closed.');
    await db.$disconnect();
    redis.disconnect();
    process.exit(0);
  });

  // Force close after 10s if dangling connections remain
  setTimeout(() => {
    logger.error('Forced shutdown due to timeout.');
    process.exit(1);
  }, 10000);
}

process.on('SIGTERM', () => handleShutdown('SIGTERM'));
process.on('SIGINT', () => handleShutdown('SIGINT'));

export default app;
