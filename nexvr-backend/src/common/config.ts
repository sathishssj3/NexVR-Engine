import dotenv from 'dotenv';
import { z } from 'zod';

dotenv.config();

const envSchema = z.object({
  NODE_ENV: z.enum(['development', 'test', 'production']).default('development'),
  PORT: z.coerce.number().default(4000),
  DATABASE_URL: z.string().default('postgresql://nexvr:nexvr_password@localhost:5432/nexvr_db?schema=public'),
  REDIS_URL: z.string().default('redis://localhost:6379'),
  JWT_SECRET: z.string().default('nexvr-enterprise-jwt-super-secret-key-change-in-prod'),
  JWT_EXPIRES_IN: z.string().default('15m'),
  JWT_REFRESH_EXPIRES_IN: z.string().default('7d'),
  AWS_REGION: z.string().default('us-east-1'),
  AWS_ACCESS_KEY_ID: z.string().optional(),
  AWS_SECRET_ACCESS_KEY: z.string().optional(),
  S3_BUCKET_NAME: z.string().default('nexvr-releases-bucket'),
  S3_ENDPOINT: z.string().optional(), // For LocalStack development
  CLOUDFRONT_DOMAIN: z.string().default('https://releases.nexvr.dev'),
  CORS_ORIGINS: z.string().default('http://localhost:3000,http://localhost:5173,https://nexvr.dev'),
  RATE_LIMIT_WINDOW_MS: z.coerce.number().default(60000),
  RATE_LIMIT_MAX: z.coerce.number().default(100),
});

export const config = envSchema.parse(process.env);
