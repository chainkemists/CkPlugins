import { BasePipeline } from 'jsr:@runreal/buildkite-ts'
import type { CommandStep } from 'jsr:@runreal/buildkite-ts'

export class CompilePipeline extends BasePipeline {
	static PIPELINE_SLUG: string = 'ckplugins-compile'

	constructor() {
		super()
		this.pipeline.agents = {
			compiler: 'true',
		}
		this.pipeline.env = {
			...this.pipeline.env,
			RUNREAL_BUILD_TS: new Date().toISOString(),
		}
	}

	build() {
		this.addCompileEditorStep()
		this.addCompileGameStep()
		return this.pipeline
	}

	private addCompileEditorStep() {
		const step: CommandStep = {
			label: 'compile game',
			command: 'runreal workflow exec compile-game-development --mode buildkite',
			id: 'compile-game',
		}
		this.addStep(step)
		this.addCommonPlugins(step)
	}

	private addCompileGameStep() {
		const step: CommandStep = {
			label: ':cooking: cook content',
			command: 'runreal workflow exec cook-all --mode buildkite',
		}
		this.addCommonPlugins(step)
		this.addStep(step)
	}
}
