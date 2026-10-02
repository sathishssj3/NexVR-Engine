import { S3Client, PutObjectCommand, GetObjectCommand } from '@aws-sdk/client-s3';
import { getSignedUrl } from '@aws-sdk/s3-request-presigner';
import { config } from './config.js';
import { logger } from './logger.js';

export const s3 = new S3Client({
  region: config.AWS_REGION,
  endpoint: config.S3_ENDPOINT,
  forcePathStyle: !!config.S3_ENDPOINT, // Required for LocalStack
  credentials:
    config.AWS_ACCESS_KEY_ID && config.AWS_SECRET_ACCESS_KEY
      ? {
          accessKeyId: config.AWS_ACCESS_KEY_ID,
          secretAccessKey: config.AWS_SECRET_ACCESS_KEY,
        }
      : undefined,
});

export async function getPresignedDownloadUrl(key: string, expiresInSeconds = 3600): Promise<string> {
  // If CloudFront is configured, return the CDN distribution URL directly
  if (config.CLOUDFRONT_DOMAIN && !config.S3_ENDPOINT) {
    return `${config.CLOUDFRONT_DOMAIN}/${key}`;
  }

  try {
    const command = new GetObjectCommand({
      Bucket: config.S3_BUCKET_NAME,
      Key: key,
    });
    return await getSignedUrl(s3, command, { expiresIn: expiresInSeconds });
  } catch (err) {
    logger.error({ err, key }, 'Failed to generate presigned download URL');
    throw err;
  }
}

export async function getPresignedUploadUrl(
  key: string,
  contentType: string,
  expiresInSeconds = 900
): Promise<string> {
  try {
    const command = new PutObjectCommand({
      Bucket: config.S3_BUCKET_NAME,
      Key: key,
      ContentType: contentType,
    });
    return await getSignedUrl(s3, command, { expiresIn: expiresInSeconds });
  } catch (err) {
    logger.error({ err, key }, 'Failed to generate presigned upload URL');
    throw err;
  }
}
